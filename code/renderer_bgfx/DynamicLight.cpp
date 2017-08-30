/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "Precompiled.h"
#pragma hdrstop

namespace renderer {

DynamicLightManager::DynamicLightManager() : nLights_(0)
{
	// Calculate the smallest square POT texture size to fit the dynamic lights data.
	const int texelSize = sizeof(float) * 4; // RGBA32F
	lightsTextureSize_ = util::CalculateSmallestPowerOfTwoTextureSize(maxLights * sizeof(DynamicLight) / texelSize);

	// FIXME: workaround d3d11 flickering texture if smaller than 64x64 and not updated every frame
	lightsTextureSize_ = std::max(uint16_t(64), lightsTextureSize_);
	interface::Printf("dlight texture size is %ux%u\n", lightsTextureSize_, lightsTextureSize_);

	// Clamp and filter are just for debug drawing. Sampling uses texel fetch.
	lightsTexture_ = bgfx::createTexture2D(lightsTextureSize_, lightsTextureSize_, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP | BGFX_TEXTURE_MIN_POINT | BGFX_TEXTURE_MAG_POINT);
}

DynamicLightManager::~DynamicLightManager()
{
	if (gridSize_.x > 0)
	{
		bgfx::destroy(cellsTexture_);
		bgfx::destroy(indicesTexture_);
	}

	bgfx::destroy(lightsTexture_);
}

void DynamicLightManager::add(uint32_t frameNo, const DynamicLight &light)
{
	if (nLights_ == maxLights - 1)
	{
		interface::PrintWarningf("Hit maximum dlights\n");
		return;
	}

	DynamicLight &l = lights_[frameNo % BGFX_NUM_BUFFER_FRAMES][nLights_++];
	l = light;
	l.color_radius.w *= g_cvars.dynamicLightScale.getFloat();
}

void DynamicLightManager::clear()
{
	nLights_ = 0;
}

void DynamicLightManager::contribute(uint32_t frameNo, vec3 position, vec3 *color, vec3 *direction) const
{
	assert(color);
	assert(direction);
	const float DLIGHT_AT_RADIUS = 16; // at the edge of a dlight's influence, this amount of light will be added
	const float DLIGHT_MINIMUM_RADIUS = 16; // never calculate a range less than this to prevent huge light numbers

	for (uint8_t i = 0; i < nLights_; i++)
	{
		const DynamicLight &dl = lights_[frameNo % BGFX_NUM_BUFFER_FRAMES][i];
		vec3 dir = dl.position_type.xyz() - position;
		float d = dir.normalize();
		float power = std::min(DLIGHT_AT_RADIUS * (dl.color_radius.a * dl.color_radius.a), DLIGHT_MINIMUM_RADIUS);
		d = power / (d * d);
		*color += util::ToGamma(dl.color_radius).rgb() * d;
		*direction += dir * d;
	}
}

void DynamicLightManager::initializeGrid()
{
	const int minCellSize = 200;
	const uint8_t maxGridSize = 32;
	const vec3 worldSize = world::GetBounds().toSize();

	for (size_t i = 0; i < 3; i++)
	{
		cellSize_[i] = std::max(minCellSize, (int)std::ceil(worldSize[i] / maxGridSize));
		gridSize_[i] = std::min(maxGridSize, (uint8_t)std::ceil(worldSize[i] / cellSize_[i]));
		cellSize_[i] = int(worldSize[i] / gridSize_[i]);
	}

	interface::Printf("dlight grid size is %ux%ux%u\n", gridSize_.x, gridSize_.y, gridSize_.z);
	gridOffset_ = vec3::empty - world::GetBounds().min;

	// Cells texture.
	cellsTextureSize_ = util::CalculateSmallestPowerOfTwoTextureSize((int)gridSize_.x * (int)gridSize_.y * (int)gridSize_.z);
	interface::Printf("dlight cells texture size is %ux%u\n", cellsTextureSize_, cellsTextureSize_);
	cellsTexture_ = bgfx::createTexture2D(cellsTextureSize_, cellsTextureSize_, false, 1, bgfx::TextureFormat::R16U, BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP | BGFX_TEXTURE_MIN_POINT | BGFX_TEXTURE_MAG_POINT);

	for (int i = 0; i < BGFX_NUM_BUFFER_FRAMES; i++)
	{
		cellsTextureData_[i].resize(cellsTextureSize_ * cellsTextureSize_);
	}

	// Indices textures.
	indicesTextureSize_ = 512;
	interface::Printf("dlight indices texture size is %ux%u\n", indicesTextureSize_, indicesTextureSize_);
	indicesTexture_ = bgfx::createTexture2D(indicesTextureSize_, indicesTextureSize_, false, 1, bgfx::TextureFormat::R8U, BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP | BGFX_TEXTURE_MIN_POINT | BGFX_TEXTURE_MAG_POINT);

	for (int i = 0; i < BGFX_NUM_BUFFER_FRAMES; i++)
	{
		indicesTextureData_[i].resize(indicesTextureSize_ * indicesTextureSize_);
	}

	assignedLights_.reserve(512); // Arbitrary initial size.
}

void DynamicLightManager::updateTextures(uint32_t frameNo)
{
	assert(world::IsLoaded());
	PROFILE_SCOPED(DynamicLightManager::updateTextures)
	const uint32_t buffer = frameNo % BGFX_NUM_BUFFER_FRAMES;

	// Assign lights to cells.
	PROFILE_BEGIN(AssignLights)
	assignedLights_.clear();
	const float cellRadius = vec3::distance(vec3::empty, vec3((float)cellSize_.x, (float)cellSize_.y, (float)cellSize_.z)) / 2.0f;

	for (uint8_t i = 0; i < nLights_; i++)
	{
		const DynamicLight &dl = lights_[buffer][i];
		vec3b min(gridSize_.x, gridSize_.y, gridSize_.z);
		vec3b max;

		// Coarse culling.
		// Get the cell positions at the sphere AABB corners.
		// The min/max will be the range of cells this light touches.
		// For capsules, use the start and end positions.
		for (int j = 0; j < (dl.position_type.w == DynamicLight::Point ? 1 : 2); j++)
		{
			Bounds aabb;

			if (j == 0)
			{
				aabb = Bounds(dl.position_type.xyz(), dl.color_radius.w);
			}
			else
			{
				assert(dl.position_type.w == DynamicLight::Capsule);
				aabb = Bounds(dl.capsuleEnd.xyz(), dl.color_radius.w);
			}

			std::array<vec3, 8> corners = aabb.toVertices();

			for (const vec3 &corner : corners)
			{
				const vec3b cellPosition = cellPositionFromWorldspacePosition(corner);

				for (int k = 0; k < 3; k++)
				{
					if (cellPosition[k] < min[k])
						min[k] = cellPosition[k];
					if (cellPosition[k] > max[k])
						max[k] = cellPosition[k];
				}
			}
		}

		for (uint8_t x = min.x; x <= max.x; x++)
		{
			for (uint8_t y = min.y; y <= max.y; y++)
			{
				for (uint8_t z = min.z; z <= max.z; z++)
				{
					// Finer grained culling.
					// Check cells against light radius for point lights.
					// Capsule lights use radius from the closest point on the capsule light segment.
					vec3 cellCenter;
					cellCenter.x = -gridOffset_.x + x * cellSize_.x + cellSize_.x / 2.0f;
					cellCenter.y = -gridOffset_.y + y * cellSize_.y + cellSize_.y / 2.0f;
					cellCenter.z = -gridOffset_.z + z * cellSize_.z + cellSize_.z / 2.0f;
					vec3 comparePosition;

					if (dl.position_type.w == DynamicLight::Point)
					{
						comparePosition = dl.position_type.xyz();
					}
					else if (dl.position_type.w == DynamicLight::Capsule)
					{
						comparePosition = math::ClosestPointOnLineSegment(dl.position_type.xyz(), dl.capsuleEnd.xyz(), cellCenter);
					}

					if (vec3::distance(cellCenter, comparePosition) > cellRadius + dl.color_radius.w)
						continue;

					assignedLights_.push_back(encodeAssignedLight(vec3b(x, y, z), i));
				}
			}
		}
	}
	PROFILE_END // AssignLights

	// Sort the assigned lights.
	std::sort(assignedLights_.begin(), assignedLights_.end());

	// Fill cells and indices texture data.
	// Make sure the first index uses num 0, so all empty cells can use it.
	memset(cellsTextureData_[buffer].data(), 0, cellsTextureData_[buffer].size() * sizeof(uint16_t));
	uint16_t indicesOffset = 0;
	indicesTextureData_[buffer][indicesOffset++] = 0; // Empty cells will point here.
	size_t currentCellIndex = 0;
	uint16_t indicesNumLightsOffset = 0;

	for (size_t i = 0; i < assignedLights_.size(); i++)
	{
		vec3b cellPosition;
		uint8_t lightIndex;
		decodeAssignedLight(assignedLights_[i], &cellPosition, &lightIndex);
		const size_t cellIndex = cellIndexFromCellPosition(cellPosition);

		// First cell, or cell index has changed?
		if (i == 0 || cellIndex != currentCellIndex)
		{
			currentCellIndex = cellIndex;

			// Point the cell to the indices.
			cellsTextureData_[buffer][cellIndex] = indicesOffset;

			// Store the offset in the indices texture where we want to write number of lights to.
			indicesNumLightsOffset = indicesOffset;

			// Initialize num lights to 0.
			indicesTextureData_[buffer][indicesNumLightsOffset] = 0;
			indicesOffset++;
		}

		// Increment num lights.
		indicesTextureData_[buffer][indicesNumLightsOffset]++;

		// Write the light index.
		indicesTextureData_[buffer][indicesOffset++] = lightIndex;

		if (indicesOffset > uint16_t(UINT16_MAX - 2))
		{
			interface::PrintWarningf("Too many assigned lights.\n");
			break;
		}
	}

	// Update the cells texture.
	bgfx::updateTexture2D(cellsTexture_, 0, 0, 0, 0, cellsTextureSize_, cellsTextureSize_, bgfx::makeRef(cellsTextureData_[buffer].data(), uint32_t(cellsTextureData_[buffer].size() * sizeof(uint16_t))));

	// Update the indices texture.
	if (nLights_ > 0 && indicesOffset > 0)
	{
		assert(indicesOffset < indicesTextureSize_ * indicesTextureSize_);
		const uint16_t width = std::min(indicesOffset, indicesTextureSize_);
		const uint16_t height = (uint16_t)std::ceil(indicesOffset / (float)indicesTextureSize_);
		bgfx::updateTexture2D(indicesTexture_, 0, 0, 0, 0, width, height, bgfx::makeRef(indicesTextureData_[buffer].data(), indicesOffset));
	}

	// Update the lights texture.
	if (nLights_ > 0)
	{
		const uint32_t size = nLights_ * sizeof(DynamicLight);
		const uint32_t texelSize = sizeof(float) * 4; // RGBA32F
		const uint16_t nTexels = uint16_t(size / texelSize);
		const uint16_t width = std::min(nTexels, lightsTextureSize_);
		const uint16_t height = (uint16_t)std::ceil(nTexels / (float)lightsTextureSize_);
		bgfx::updateTexture2D(lightsTexture_, 0, 0, 0, 0, width, height, bgfx::makeRef(lights_[buffer], size));
	}
}

void DynamicLightManager::updateUniforms(Uniforms *uniforms)
{
	assert(uniforms);
	uniforms->dynamicLightCellSize.set(vec4((float)cellSize_.x, (float)cellSize_.y, (float)cellSize_.z, (float)cellsTextureSize_));
	uniforms->dynamicLightGridOffset.set(gridOffset_);
	uniforms->dynamicLightGridSize.set(vec4((float)gridSize_.x, (float)gridSize_.y, (float)gridSize_.z, 0));
	uniforms->dynamicLight_Num_Intensity.set(vec4((float)nLights_, g_cvars.dynamicLightIntensity.getFloat(), 0, 0));
	uniforms->dynamicLightTextureSizes_Cells_Indices_Lights.set(vec4((float)cellsTextureSize_, (float)indicesTextureSize_, (float)lightsTextureSize_, 0));
}

void DynamicLightManager::decodeAssignedLight(uint32_t value, vec3b *cellPosition, uint8_t *lightIndex) const
{
	assert(cellPosition);
	assert(lightIndex);
	cellPosition->x = (value >> 24) & 0xff;
	cellPosition->y = (value >> 16) & 0xff;
	cellPosition->z = (value >> 8) & 0xff;
	*lightIndex = value & 0xff;
}

uint32_t DynamicLightManager::encodeAssignedLight(vec3b cellPosition, uint8_t lightIndex) const
{
	return (cellPosition.x << 24) + (cellPosition.y << 16) + (cellPosition.z << 8) + lightIndex;
}

size_t DynamicLightManager::cellIndexFromCellPosition(vec3b position) const
{
	return position.x + (position.y * (size_t)gridSize_.x) + (position.z * (size_t)gridSize_.x * (size_t)gridSize_.y);
}

vec3b DynamicLightManager::cellPositionFromWorldspacePosition(vec3 position) const
{
	const vec3 local = gridOffset_ + position;
	vec3b cellPosition;
	cellPosition.x = std::min(uint8_t(std::max(0.0f, local.x / cellSize_.x)), uint8_t(gridSize_.x - 1));
	cellPosition.y = std::min(uint8_t(std::max(0.0f, local.y / cellSize_.y)), uint8_t(gridSize_.y - 1));
	cellPosition.z = std::min(uint8_t(std::max(0.0f, local.z / cellSize_.z)), uint8_t(gridSize_.z - 1));
	return cellPosition;
}

} // namespace renderer
