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

Material::Material(const char *name)
{
	util::Strncpyz(this->name, name, sizeof(this->name));
}

void Material::finish()
{
	for (size_t i = 0; i < maxStages; i++)
	{
		stages[i].material = this;
	}

	if (isSky)
		sort = MaterialSort::Environment;

	if (polygonOffset && !sort)
		sort = MaterialSort::Decal;

	// set appropriate stage information
	bool hasLightmapStage = false;
	int stageIndex;

	for (stageIndex = 0; stageIndex < maxStages;)
	{
		MaterialStage *pStage = &stages[stageIndex];

		if (!pStage->active)
			break;

		// check for a missing texture
		if (!pStage->bundles[0].textures[0])
		{
			interface::PrintWarningf("Material %s has a stage with no image\n", name);
			pStage->active = false;
			stageIndex++;
			continue;
		}

		// default texture coordinate generation
		if (pStage->bundles[0].isLightmap)
		{
			if (pStage->bundles[0].tcGen == MaterialTexCoordGen::None)
			{
				pStage->bundles[0].tcGen = MaterialTexCoordGen::Lightmap;
			}
			
			hasLightmapStage = true;
		}
		else
		{
			if (pStage->bundles[0].tcGen == MaterialTexCoordGen::None)
			{
				pStage->bundles[0].tcGen = MaterialTexCoordGen::Texture;
			}
		}

		// determine sort order and fog color adjustment
		if ((pStage->blendSrc != 0 || pStage->blendDst != 0) && (stages[0].blendSrc != 0 || stages[0].blendDst != 0))
		{
			// fog color adjustment only works for blend modes that have a contribution
			// that aproaches 0 as the modulate values aproach 0 --
			// GL_ONE, GL_ONE
			// GL_ZERO, GL_ONE_MINUS_SRC_COLOR
			// GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

			// modulate, additive
			if ((pStage->blendSrc == BGFX_STATE_BLEND_ONE && pStage->blendDst == BGFX_STATE_BLEND_ONE) || (pStage->blendSrc == BGFX_STATE_BLEND_ZERO && pStage->blendDst == BGFX_STATE_BLEND_INV_SRC_COLOR))
			{
				pStage->adjustColorsForFog = MaterialAdjustColorsForFog::ModulateRGB;
			}
			// strict blend
			else if (pStage->blendSrc == BGFX_STATE_BLEND_SRC_ALPHA && pStage->blendDst == BGFX_STATE_BLEND_INV_SRC_ALPHA)
			{
				pStage->adjustColorsForFog = MaterialAdjustColorsForFog::ModulateAlpha;
			}
			// premultiplied alpha
			else if (pStage->blendSrc == BGFX_STATE_BLEND_ONE && pStage->blendDst == BGFX_STATE_BLEND_INV_SRC_ALPHA)
			{
				pStage->adjustColorsForFog = MaterialAdjustColorsForFog::ModulateRGBA;
			}
			else
			{
				// we can't adjust this one correctly, so it won't be exactly correct in fog
			}

			// don't screw with sort order if this is a portal or environment
			if (!sort)
			{
				// see through item, like a grill or grate
				if (pStage->depthWrite)
				{
					sort = MaterialSort::SeeThrough;
				}
				else
				{
					sort = MaterialSort::Blend0;
				}
			}
		}
		
		stageIndex++;
	}

	// there are times when you will need to manually apply a sort to opaque alpha tested shaders that have later blend passes
	if (!sort)
		sort = MaterialSort::Opaque;

	stageIndex = collapseStagesToGLSL();

	if (lightmapIndex >= 0 && !hasLightmapStage)
	{
		interface::PrintDeveloperf("WARNING: material '%s' has lightmap but no lightmap stage!\n", name);
	}

	// compute number of passes
	numUnfoggedPasses = stageIndex;

	// fogonly shaders don't have any normal passes
	if (stageIndex == 0 && !isSky)
		sort = MaterialSort::Fog;

	if (sort <= MaterialSort::Opaque)
	{
		fogPass = MaterialFogPass::Equal;
	}
	else if (contentFlags & CONTENTS_FOG)
	{
		fogPass = MaterialFogPass::LessOrEqual;
	}
}

int Material::collapseStagesToGLSL()
{
	int i, j, numStages;

	for (i = 0; i < maxStages; i++)
	{
		MaterialStage &stage = stages[i];

		if (!stage.active)
			continue;

		if (stage.rgbGen == MaterialColorGen::LightingDiffuse)
		{
			stage.light = MaterialLight::Vector;
		}
		else if (stage.rgbGen == MaterialColorGen::VertexLit || stage.rgbGen == MaterialColorGen::ExactVertexLit)
		{
			stage.light = MaterialLight::Vertex;
		}
	}

	// if 2+ stages and first stage is lightmap, switch them
	// this makes it easier for the later bits to process
	if (stages[0].active && stages[0].bundles[0].tcGen == MaterialTexCoordGen::Lightmap && stages[1].active)
	{
		if ((stages[1].blendSrc == BGFX_STATE_BLEND_ZERO && stages[1].blendDst == BGFX_STATE_BLEND_SRC_COLOR) ||
			(stages[1].blendSrc == BGFX_STATE_BLEND_DST_COLOR && stages[1].blendDst == BGFX_STATE_BLEND_ZERO))
		{
			const uint64_t depthTestBits0 = stages[0].depthTestBits;
			const bool depthWrite0 = stages[0].depthWrite;
			const MaterialAlphaTest alphaTest0 = stages[0].alphaTest;
			const uint64_t blendSrc0 = stages[0].blendSrc;
			const uint64_t blendDst0 = stages[0].blendDst;
			const uint64_t depthTestBits1 = stages[1].depthTestBits;
			const bool depthWrite1 = stages[1].depthWrite;
			const MaterialAlphaTest alphaTest1 = stages[1].alphaTest;
			const uint64_t blendSrc1 = stages[1].blendSrc;
			const uint64_t blendDst1 = stages[1].blendDst;
				
			MaterialStage swapStage = stages[0];
			stages[0] = stages[1];
			stages[1] = swapStage;

			stages[0].depthTestBits = depthTestBits0;
			stages[0].depthWrite = depthWrite0;
			stages[0].alphaTest = alphaTest0;
			stages[0].blendSrc = blendSrc0;
			stages[0].blendDst = blendDst0;
			stages[1].depthTestBits = depthTestBits1;
			stages[1].depthWrite = depthWrite1;
			stages[1].alphaTest = alphaTest1;
			stages[1].blendSrc = blendSrc1;
			stages[1].blendDst = blendDst1;
		}
	}

	bool skip = false;

	// scan for shaders that aren't supported
	for (i = 0; i < maxStages; i++)
	{
		MaterialStage *pStage = &stages[i];

		if (!pStage->active)
			continue;

		if (pStage->adjustColorsForFog != MaterialAdjustColorsForFog::None)
		{
			skip = true;
			break;
		}

		if (pStage->bundles[0].tcGen == MaterialTexCoordGen::Lightmap)
		{
			if (pStage->blendSrc != BGFX_STATE_BLEND_ZERO && pStage->blendDst != BGFX_STATE_BLEND_ZERO && pStage->blendSrc != BGFX_STATE_BLEND_DST_COLOR && pStage->blendDst != BGFX_STATE_BLEND_SRC_COLOR)
			{
				skip = true;
				break;
			}
		}

		switch (pStage->bundles[0].tcGen)
		{
			case MaterialTexCoordGen::Texture:
			case MaterialTexCoordGen::Lightmap:
			case MaterialTexCoordGen::EnvironmentMapped:
			case MaterialTexCoordGen::Vector:
				break;
			default:
				skip = true;
				break;
		}

		switch(pStage->alphaGen)
		{
			case MaterialAlphaGen::LightingSpecular:
			case MaterialAlphaGen::Portal:
				skip = true;
				break;
			default:
				break;
		}
	}

	if (!skip)
	{
		for (i = 0; i < maxStages; i++)
		{
			MaterialStage *pStage = &stages[i];
			MaterialStage *diffuse, *normal, *specular, *lightmap;
			bool parallax;

			if (!pStage->active)
				continue;

			// skip normal and specular maps
			if (pStage->type != MaterialStageType::ColorMap)
				continue;

			// skip lightmaps
			if (pStage->bundles[0].tcGen == MaterialTexCoordGen::Lightmap)
				continue;

			diffuse = pStage;
			normal = NULL;
			parallax = false;
			specular = NULL;
			lightmap = NULL;

			// we have a diffuse map, find matching normal, specular, and lightmap
			for (j = i + 1; j < maxStages; j++)
			{
				MaterialStage *pStage2 = &stages[j];

				if (!pStage2->active)
					continue;

				switch(pStage2->type)
				{
					case MaterialStageType::NormalMap:
						if (!normal)
						{
							normal = pStage2;
						}
						break;

					case MaterialStageType::NormalParallaxMap:
						if (!normal)
						{
							normal = pStage2;
							parallax = true;
						}
						break;

					case MaterialStageType::SpecularMap:
						if (!specular)
						{
							specular = pStage2;
						}
						break;

					case MaterialStageType::ColorMap:
						if (pStage2->bundles[0].tcGen == MaterialTexCoordGen::Lightmap)
						{
							lightmap = pStage2;
						}
						break;

					default:
						break;
				}
			}

			if (lightmap)
			{
				diffuse->bundles[MaterialTextureBundleIndex::Lightmap] = lightmap->bundles[0];
				diffuse->light = MaterialLight::Map;
			}
		}

		// deactivate lightmap stages
		for (i = 0; i < maxStages; i++)
		{
			MaterialStage *pStage = &stages[i];

			if (!pStage->active)
				continue;

			if (pStage->bundles[0].tcGen == MaterialTexCoordGen::Lightmap)
			{
				pStage->active = false;
			}
		}
	}

	// deactivate normal and specular stages
	for (i = 0; i < maxStages; i++)
	{
		MaterialStage *pStage = &stages[i];

		if (!pStage->active)
			continue;

		if (pStage->type == MaterialStageType::NormalMap)
		{
			pStage->active = false;
		}

		if (pStage->type == MaterialStageType::NormalParallaxMap)
		{
			pStage->active = false;
		}

		if (pStage->type == MaterialStageType::SpecularMap)
		{
			pStage->active = false;
		}			
	}

	// remove inactive stages
	numStages = 0;

	for (i = 0; i < maxStages; i++)
	{
		if (!stages[i].active)
			continue;

		if (i == numStages)
		{
			numStages++;
			continue;
		}

		stages[numStages] = stages[i];
		stages[i].active = false;
		numStages++;
	}

	// convert any remaining lightmap stages to a lighting pass with a white texture
	if (numDeforms == 0)
	{
		for (i = 0; i < maxStages; i++)
		{
			MaterialStage *pStage = &stages[i];

			if (!pStage->active)
				continue;

			if (pStage->adjustColorsForFog != MaterialAdjustColorsForFog::None)
				continue;

			if (pStage->bundles[MaterialTextureBundleIndex::DiffuseMap].tcGen == MaterialTexCoordGen::Lightmap)
			{
				pStage->light = MaterialLight::Map;
				pStage->bundles[MaterialTextureBundleIndex::Lightmap] = pStage->bundles[MaterialTextureBundleIndex::DiffuseMap];
				pStage->bundles[MaterialTextureBundleIndex::DiffuseMap].textures[0] = g_textureCache->getWhite();
				pStage->bundles[MaterialTextureBundleIndex::DiffuseMap].isLightmap = false;
				pStage->bundles[MaterialTextureBundleIndex::DiffuseMap].tcGen = MaterialTexCoordGen::Texture;
			}
		}
	}

	// convert any remaining lightingdiffuse stages to a lighting pass
	if (numDeforms == 0)
	{
		for (i = 0; i < maxStages; i++)
		{
			MaterialStage *pStage = &stages[i];

			if (!pStage->active)
				continue;

			if (pStage->adjustColorsForFog != MaterialAdjustColorsForFog::None)
				continue;

			if (pStage->rgbGen == MaterialColorGen::LightingDiffuse)
			{
				pStage->light = MaterialLight::Vector;
			}
		}
	}

	return numStages;
}

} // namespace renderer
