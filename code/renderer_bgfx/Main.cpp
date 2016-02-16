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
/*
 * Copyright 2011-2015 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */
#include "Precompiled.h"
#pragma hdrstop

#include "../jo_jpeg.cpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Main.h"

namespace renderer {

std::array<Vertex *, 4> ExtractQuadCorners(Vertex *vertices, const uint16_t *indices)
{
	std::array<uint16_t, 6> sorted;
	memcpy(sorted.data(), indices, sizeof(uint16_t) * sorted.size());
	std::sort(sorted.begin(), sorted.end());
	std::array<Vertex *, 4> corners;
	size_t cornerIndex = 0;

	for (size_t i = 0; i < sorted.size(); i++)
	{
		if (i == 0 || sorted[i] != sorted[i - 1])
			corners[cornerIndex++] = &vertices[sorted[i]];
	}

	assert(cornerIndex == 4); // Should be exactly 4 unique vertices.
	return corners;
}

void BgfxCallback::fatal(bgfx::Fatal::Enum _code, const char* _str)
{
	if (bgfx::Fatal::DebugCheck == _code)
	{
		bx::debugBreak();
	}
	else
	{
		BX_TRACE("0x%08x: %s", _code, _str);
		BX_UNUSED(_code, _str);
		abort();
	}
}

void BgfxCallback::traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList)
{
	char temp[2048];
	char* out = temp;
	int32_t len   = bx::snprintf(out, sizeof(temp), "%s (%d): ", _filePath, _line);
	int32_t total = len + bx::vsnprintf(out + len, sizeof(temp)-len, _format, _argList);
	if ( (int32_t)sizeof(temp) < total)
	{
		out = (char*)alloca(total+1);
		memcpy(out, temp, len);
		bx::vsnprintf(out + len, total-len, _format, _argList);
	}
	out[total] = '\0';
	bx::debugOutput(out);
}

uint32_t BgfxCallback::cacheReadSize(uint64_t _id)
{
	return 0;
}

bool BgfxCallback::cacheRead(uint64_t _id, void* _data, uint32_t _size)
{
	return false;
}

void BgfxCallback::cacheWrite(uint64_t _id, const void* _data, uint32_t _size)
{
}

struct ImageWriteBuffer
{
	std::vector<uint8_t> *data;
	size_t bytesWritten;
};

static void ImageWriteCallback(void *context, void *data, int size)
{
	auto buffer = (ImageWriteBuffer *)context;

	if (buffer->data->size() < buffer->bytesWritten + size)
	{
		buffer->data->resize(buffer->bytesWritten + size);
	}

	memcpy(&buffer->data->data()[buffer->bytesWritten], data, size);
	buffer->bytesWritten += size;
}

static void ImageWriteCallbackConst(void *context, const void *data, int size)
{
	ImageWriteCallback(context, (void *)data, size);
}

void BgfxCallback::screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t _size, bool _yflip)
{
	const int nComponents = 4;
	const char *extension = util::GetExtension(_filePath);
	const bool writeAsPng = !util::Stricmp(extension, "png");
	const size_t outputPitch = writeAsPng ? _pitch : _width * nComponents; // PNG can use any pitch, others can't.

	// Convert from BGRA to RGBA, and flip y if needed.
	const size_t requiredSize = outputPitch * _height;

	if (screenShotDataBuffer_.size() < requiredSize)
	{
		screenShotDataBuffer_.resize(requiredSize);
	}

	for (uint32_t y = 0; y < _height; y++)
	{
		for (uint32_t x = 0; x < _width; x++)
		{
			auto colorIn = &((const uint8_t *)_data)[x * nComponents + (_yflip ? _height - 1 - y : y) * _pitch];
			uint8_t *colorOut = &screenShotDataBuffer_[x * nComponents + y * outputPitch];
			colorOut[0] = colorIn[2];
			colorOut[1] = colorIn[1];
			colorOut[2] = colorIn[0];
			colorOut[3] = colorIn[3];
		}
	}

	// Write to file buffer.
	ImageWriteBuffer buffer;
	buffer.data = &screenShotFileBuffer_;
	buffer.bytesWritten = 0;

	if (writeAsPng)
	{
		if (!stbi_write_png_to_func(ImageWriteCallback, &buffer, _width, _height, nComponents, screenShotDataBuffer_.data(), outputPitch))
		{
			ri.Printf(PRINT_ALL, "Screenshot: error writing png file\n");
			return;
		}
	}
	else if (!util::Stricmp(extension, "jpg"))
	{
		if (!jo_write_jpg_to_func(ImageWriteCallbackConst, &buffer, screenShotDataBuffer_.data(), _width, _height, nComponents, g_cvars.screenshotJpegQuality->integer))
		{
			ri.Printf(PRINT_ALL, "Screenshot: error writing jpg file\n");
			return;
		}
	}
	else
	{
		if (!stbi_write_tga_to_func(ImageWriteCallback, &buffer, _width, _height, nComponents, screenShotDataBuffer_.data()))
		{
			ri.Printf(PRINT_ALL, "Screenshot: error writing tga file\n");
			return;
		}
	}

	// Write file buffer to file.
	if (buffer.bytesWritten > 0)
	{
		ri.FS_WriteFile(_filePath, buffer.data->data(), buffer.bytesWritten);
	}
}

void BgfxCallback::captureBegin(uint32_t _width, uint32_t _height, uint32_t _pitch, bgfx::TextureFormat::Enum _format, bool _yflip)
{
}

void BgfxCallback::captureEnd()
{
}

void BgfxCallback::captureFrame(const void* _data, uint32_t _size)
{
}

bool DrawCall::operator<(const DrawCall &other) const
{
	assert(material);
	assert(other.material);

	if (material->sort < other.material->sort)
	{
		return true;
	}
	else if (material->sort == other.material->sort)
	{
		if (sort < other.sort)
		{
			return true;
		}
		else if (sort == other.sort)
		{
			if (material->index < other.material->index)
				return true;
		}
	}

	return false;
}

DynamicLightManager::DynamicLightManager() : nLights_(0)
{
	// Calculate the smallest square POT texture size to fit the dynamic lights data.
	const int texelSize = sizeof(float) * 4; // RGBA32F
	const int sr = (int)ceil(sqrtf(maxLights * sizeof(DynamicLight) / texelSize));
	textureSize_ = 1;

	while (textureSize_ < sr)
		textureSize_ *= 2;

	// FIXME: workaround d3d11 flickering texture if smaller than 64x64 and not updated every frame
	textureSize_ = std::max(64, textureSize_);

	// Clamp and filter are just for debug drawing. Sampling uses texel fetch.
	texture_ = bgfx::createTexture2D(textureSize_, textureSize_, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP | BGFX_TEXTURE_MIN_POINT | BGFX_TEXTURE_MAG_POINT);
}

DynamicLightManager::~DynamicLightManager()
{
	bgfx::destroyTexture(texture_);
}

void DynamicLightManager::add(int frameNo, const DynamicLight &light)
{
	if (nLights_ == maxLights)
	{
		ri.Printf(PRINT_WARNING, "Hit maximum dlights\n");
		return;
	}

	lights_[frameNo % BGFX_NUM_BUFFER_FRAMES][nLights_++] = light;
}

void DynamicLightManager::clear()
{
	nLights_ = 0;
}

void DynamicLightManager::contribute(int frameNo, vec3 position, vec3 *color, vec3 *direction) const
{
	assert(color);
	assert(direction);
	const float DLIGHT_AT_RADIUS = 16; // at the edge of a dlight's influence, this amount of light will be added
	const float DLIGHT_MINIMUM_RADIUS = 16; // never calculate a range less than this to prevent huge light numbers

	for (size_t i = 0; i < nLights_; i++)
	{
		const DynamicLight &dl = lights_[frameNo % BGFX_NUM_BUFFER_FRAMES][i];
		vec3 dir = dl.position_type.xyz() - position;
		float d = dir.normalize();
		float power = std::min(DLIGHT_AT_RADIUS * (dl.color_radius.a * dl.color_radius.a), DLIGHT_MINIMUM_RADIUS);
		d = power / (d * d);
		*color += dl.color_radius.rgb() * d;
		*direction += dir * d;
	}
}

void DynamicLightManager::update(int frameNo, Uniform_vec4 *uniform)
{
	assert(uniform);
	uniform->set(vec4((float)nLights_, (float)textureSize_, 0, 0));
	
	if (nLights_ > 0)
	{
		const uint32_t size = nLights_ * sizeof(DynamicLight);
		const uint32_t texelSize = sizeof(float) * 4; // RGBA32F
		const uint16_t nTexels = uint16_t(size / texelSize);
		const uint16_t width = std::min(nTexels, uint16_t(textureSize_));
		const uint16_t height = (uint16_t)std::ceil(nTexels / (float)textureSize_);
		bgfx::updateTexture2D(texture_, 0, 0, 0, width, height, bgfx::makeRef(lights_[frameNo % BGFX_NUM_BUFFER_FRAMES], size));
	}
}

#define NOISE_PERM(a) noisePerm_[(a) & (noiseSize_ - 1)]
#define NOISE_TABLE(x, y, z, t) noiseTable_[NOISE_PERM(x + NOISE_PERM(y + NOISE_PERM(z + NOISE_PERM(t))))]
#define NOISE_LERP( a, b, w ) ( ( a ) * ( 1.0f - ( w ) ) + ( b ) * ( w ) )

float Main::getNoise(float x, float y, float z, float t) const
{
	int i;
	int ix, iy, iz, it;
	float fx, fy, fz, ft;
	float front[4];
	float back[4];
	float fvalue, bvalue, value[2], finalvalue;

	ix = (int)floor(x);
	fx = x - ix;
	iy = (int)floor(y);
	fy = y - iy;
	iz = (int)floor(z);
	fz = z - iz;
	it = (int)floor(t);
	ft = t - it;

	for (i = 0; i < 2; i++)
	{
		front[0] = NOISE_TABLE(ix, iy, iz, it + i);
		front[1] = NOISE_TABLE(ix + 1, iy, iz, it + i);
		front[2] = NOISE_TABLE(ix, iy + 1, iz, it + i);
		front[3] = NOISE_TABLE(ix + 1, iy + 1, iz, it + i);

		back[0] = NOISE_TABLE(ix, iy, iz + 1, it + i);
		back[1] = NOISE_TABLE(ix + 1, iy, iz + 1, it + i);
		back[2] = NOISE_TABLE(ix, iy + 1, iz + 1, it + i);
		back[3] = NOISE_TABLE(ix + 1, iy + 1, iz + 1, it + i);

		fvalue = NOISE_LERP(NOISE_LERP(front[0], front[1], fx), NOISE_LERP(front[2], front[3], fx), fy);
		bvalue = NOISE_LERP(NOISE_LERP(back[0], back[1], fx), NOISE_LERP(back[2], back[3], fx), fy);

		value[i] = NOISE_LERP(fvalue, bvalue, fz);
	}

	finalvalue = NOISE_LERP(value[0], value[1], ft);

	return finalvalue;
}

void Main::debugPrint(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	char text[1024];
	util::Vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	bgfx::dbgTextPrintf(0, debugTextY, 0x4f, text);
	debugTextY++;
}

void Main::drawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex)
{
	auto mat = materialCache_->getMaterial(materialIndex);

	if (stretchPicMaterial_ != mat)
	{
		flushStretchPics();
		stretchPicMaterial_ = mat;
	}

	auto firstVertex = (const uint16_t)stretchPicVertices_.size();
	size_t firstIndex = stretchPicIndices_.size();
	stretchPicVertices_.resize(stretchPicVertices_.size() + 4);
	stretchPicIndices_.resize(stretchPicIndices_.size() + 6);
	Vertex *v = &stretchPicVertices_[firstVertex];
	uint16_t *i = &stretchPicIndices_[firstIndex];
	v[0].pos = vec3(x, y, 0);
	v[1].pos = vec3(x + w, y, 0);
	v[2].pos = vec3(x + w, y + h, 0);
	v[3].pos = vec3(x, y + h, 0);
	v[0].texCoord = vec2(s1, t1);
	v[1].texCoord = vec2(s2, t1);
	v[2].texCoord = vec2(s2, t2);
	v[3].texCoord = vec2(s1, t2);
	v[0].color = v[1].color = v[2].color = v[3].color = stretchPicColor_;
	i[0] = firstVertex + 3; i[1] = firstVertex + 0; i[2] = firstVertex + 2;
	i[3] = firstVertex + 2; i[4] = firstVertex + 0; i[5] = firstVertex + 1;
}

void Main::drawStretchRaw(int x, int y, int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty)
{
	if (!math::IsPowerOfTwo(cols) || !math::IsPowerOfTwo(rows))
	{
		ri.Error(ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, 4, &tib, 6))
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	flushStretchPics();
	stretchPicViewId_ = UINT8_MAX;
	uploadCinematic(w, h, cols, rows, data, client, dirty);
	auto vertices = (Vertex *)tvb.data;
	vertices[0].pos = { 0, 0, 0 }; vertices[0].texCoord = { 0, 0 }; vertices[0].color = vec4::white;
	vertices[1].pos = { 1, 0, 0 }; vertices[1].texCoord = { 1, 0 }; vertices[1].color = vec4::white;
	vertices[2].pos = { 1, 1, 0 }; vertices[2].texCoord = { 1, 1 }; vertices[2].color = vec4::white;
	vertices[3].pos = { 0, 1, 0 }; vertices[3].texCoord = { 0, 1 }; vertices[3].color = vec4::white;
	auto indices = (uint16_t *)tib.data;
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 2; indices[4] = 3; indices[5] = 0;
	bgfx::setVertexBuffer(&tvb);
	bgfx::setIndexBuffer(&tib);
	bgfx::setTexture(0, matStageUniforms_->diffuseMap.handle, textureCache_->getScratchTextures()[client]->getHandle());
	matStageUniforms_->color.set(vec4::white);
	bgfx::setState(BGFX_STATE_RGB_WRITE);
	const uint8_t viewId = pushView(defaultFb_, BGFX_CLEAR_NONE, mat4::identity, mat4::orthographicProjection(0, 1, 0, 1, -1, 1), Rect(x, y, w, h), PushViewFlags::Sequential);
	bgfx::submit(viewId, shaderPrograms_[ShaderProgramId::TextureColor].handle);
}

void Main::uploadCinematic(int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty)
{
	auto scratch = textureCache_->getScratchTextures()[client];
	
	if (cols != scratch->getWidth() || rows != scratch->getHeight())
	{
		scratch->resize(cols, rows);
		dirty = true;
	}

	if (dirty)
	{
		auto mem = bgfx::alloc(cols * rows * 4);
		memcpy(mem->data, data, mem->size);
		scratch->update(mem, 0, 0, cols, rows);
	}
}

void Main::loadWorld(const char *name)
{
	if (world::IsLoaded())
	{
		ri.Error(ERR_DROP, "ERROR: attempted to redundantly load world map");
	}

	world::Load(name);
	mainVisCacheId_ = world::CreateVisCache();
	portalVisCacheId_ = world::CreateVisCache();
}

void Main::addDynamicLightToScene(const DynamicLight &light)
{
	dlightManager_->add(frameNo_, light);
}

void Main::addEntityToScene(const refEntity_t *entity)
{
	sceneEntities_.push_back({ *entity });

	// Hack in extra dlights for Quake 3 content.
	const vec3 bfgColor(0.08f, 1.0f, 0.4f);
	const vec3 plasmaColor(0.6f, 0.6f, 1.0f);
	DynamicLight dl;
	dl.color_radius = vec4::empty;
	dl.position_type = vec4(entity->origin, DynamicLight::Point);

	// BFG projectile.
	if (entity->reType == RT_MODEL && bfgMissibleModel_ && entity->hModel == bfgMissibleModel_->getIndex())
	{
		dl.color_radius = vec4(bfgColor, 200); // Same radius as rocket.
	}
	// BFG explosion.
	else if (entity->reType == RT_SPRITE && bfgExplosionMaterial_ && entity->customShader == bfgExplosionMaterial_->index)
	{
		dl.color_radius = vec4(bfgColor, 300 * calculateExplosionLight(entity->shaderTime, 1000)); // Same radius and duration as rocket explosion.
	}
	// Lightning bolt.
	else if (entity->reType == RT_LIGHTNING)
	{
		const float base = 1;
		const float amplitude = 0.1f;
		const float phase = 0;
		const float freq = 10.1f;
		const float radius = base + g_sinTable[ri.ftol((phase + floatTime_ * freq) * g_funcTableSize) & g_funcTableMask] * amplitude;
		dl.capsuleEnd = vec3(entity->oldorigin);
		dl.color_radius = vec4(0.6f, 0.6f, 1, 150 * radius);
		dl.position_type.w = DynamicLight::Capsule;
	}
	// Plasma ball.
	else if (entity->reType == RT_SPRITE && plasmaBallMaterial_ && entity->customShader == plasmaBallMaterial_->index)
	{
		dl.color_radius = vec4(plasmaColor, 100);
	}
	// Plasma explosion.
	else if (entity->reType == RT_MODEL && plasmaExplosionMaterial_ && entity->customShader == plasmaExplosionMaterial_->index)
	{
		dl.color_radius = vec4(plasmaColor, 200 * calculateExplosionLight(entity->shaderTime, 600)); // CG_MissileHitWall: 600ms duration.
	}
	// Rail core.
	else if (entity->reType == RT_RAIL_CORE)
	{
		dl.capsuleEnd = vec3(entity->oldorigin);
		dl.color_radius = vec4(vec4::fromBytes(entity->shaderRGBA).xyz(), 150);
		dl.position_type.w = DynamicLight::Capsule;
	}

	if (dl.color_radius.a > 0)
	{
		addDynamicLightToScene(dl);
	}
}

void Main::addPolyToScene(qhandle_t hShader, int nVerts, const polyVert_t *verts, int nPolys)
{
	const size_t firstVertex = scenePolygonVertices_.size();
	scenePolygonVertices_.insert(scenePolygonVertices_.end(), verts, &verts[nPolys * nVerts]);

	for (int i = 0; i < nPolys; i++)
	{
		Polygon p;
		p.material = materialCache_->getMaterial(hShader); 
		p.firstVertex = firstVertex + i * nVerts;
		p.nVertices = nVerts;
		Bounds bounds;
		bounds.setupForAddingPoints();

		for (size_t j = 0; j < p.nVertices; j++)
		{
			bounds.addPoint(scenePolygonVertices_[p.firstVertex + j].xyz);
		}

		p.fogIndex = world::FindFogIndex(bounds);
		scenePolygons_.push_back(p);
	}
}

void Main::renderScene(const refdef_t *def)
{
	flushStretchPics();
	stretchPicViewId_ = UINT8_MAX;
	time_ = def->time;
	floatTime_ = def->time * 0.001f;
	
	// Clamp view rect to screen.
	const int x = std::max(0, def->x);
	const int y = std::max(0, def->y);
	const int w = std::min(glConfig.vidWidth, x + def->width) - x;
	const int h = std::min(glConfig.vidHeight, y + def->height) - y;

	if (def->rdflags & RDF_HYPERSPACE)
	{
		const uint8_t c = time_ & 255;
		const uint8_t viewId = pushView(defaultFb_, 0, mat4::identity, mat4::identity, Rect(x, y, w, h));
		bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, (c<<24)|(c<<16)|(c<<8)|0xff);
		bgfx::touch(viewId);
	}
	else
	{
		const vec3 scenePosition(def->vieworg);
		sceneRotation_ = mat3(def->viewaxis);
		isWorldCamera_ = (def->rdflags & RDF_NOWORLDMODEL) == 0 && world::IsLoaded();
		renderCamera(mainVisCacheId_, scenePosition, scenePosition, sceneRotation_, Rect(x, y, w, h), vec2(def->fov_x, def->fov_y), def->areamask);

		// HDR.
		if (g_cvars.hdr->integer && isWorldCamera_)
		{
			// Luminance.
			for (size_t i = 0; i < nLuminanceFrameBuffers_; i++)
			{
				ShaderProgramId::Enum programId;

				if (i == 0)
				{
					programId = ShaderProgramId::Luminance;
					setTexelOffsetsDownsample2x2(luminanceFrameBufferSizes_[i], luminanceFrameBufferSizes_[i]);
					bgfx::setTexture(MaterialTextureBundleIndex::DiffuseMap, matStageUniforms_->diffuseMap.handle, sceneFbColor_);
				}
				else
				{
					programId = ShaderProgramId::LuminanceDownsample;
					setTexelOffsetsDownsample4x4(luminanceFrameBufferSizes_[i], luminanceFrameBufferSizes_[i]);
					bgfx::setTexture(MaterialTextureBundleIndex::DiffuseMap, matStageUniforms_->diffuseMap.handle, luminanceFrameBuffers_[i - 1].handle);
				}

				renderScreenSpaceQuad(luminanceFrameBuffers_[i], programId, BGFX_STATE_RGB_WRITE, isTextureOriginBottomLeft_, Rect(0, 0, luminanceFrameBufferSizes_[i], luminanceFrameBufferSizes_[i]));
			}

			// Luminance adaptation.
			if (lastAdaptedLuminanceTime_ <= 0)
			{
				lastAdaptedLuminanceTime_ = floatTime_;
			}

			matUniforms_->time.set(vec4(floatTime_ - lastAdaptedLuminanceTime_, 0, 0, 0));
			lastAdaptedLuminanceTime_ = floatTime_;
			bgfx::setTexture(MaterialTextureBundleIndex::Luminance, matStageUniforms_->luminanceSampler.handle, luminanceFrameBuffers_[nLuminanceFrameBuffers_ - 1].handle);
			bgfx::setTexture(MaterialTextureBundleIndex::AdaptedLuminance, matStageUniforms_->adaptedLuminanceSampler.handle, adaptedLuminanceFB_[1 - currentAdaptedLuminanceFB_].handle);
			renderScreenSpaceQuad(adaptedLuminanceFB_[currentAdaptedLuminanceFB_], ShaderProgramId::AdaptedLuminance, BGFX_STATE_RGB_WRITE, isTextureOriginBottomLeft_);
			currentAdaptedLuminanceFB_ = 1 - currentAdaptedLuminanceFB_;

			// Tonemap.
			// Clamp to sane values.
			uniforms_->brightnessContrastGammaSaturation.set(vec4
			(
				Clamped(g_cvars.brightness->value - 1.0f, -0.8f, 0.8f),
				Clamped(g_cvars.contrast->value, 0.5f, 3.0f),
				Clamped(g_cvars.gamma->value, 0.5f, 3.0f),
				Clamped(g_cvars.saturation->value, 0.0f, 3.0f)
			));

			uniforms_->hdrKey.set(vec4(Clamped(g_cvars.hdrKey->value, 0.01f, 0.2f), 0, 0, 0));
			bgfx::setTexture(MaterialTextureBundleIndex::DiffuseMap, matStageUniforms_->diffuseMap.handle, sceneFbColor_);
			bgfx::setTexture(MaterialTextureBundleIndex::AdaptedLuminance, matStageUniforms_->adaptedLuminanceSampler.handle, adaptedLuminanceFB_[currentAdaptedLuminanceFB_].handle);
			renderScreenSpaceQuad(aa_ == AntiAliasing::FXAA ? fxaaFb_ : defaultFb_, ShaderProgramId::ToneMap, BGFX_STATE_RGB_WRITE, isTextureOriginBottomLeft_);
		}
		else if (isWorldCamera_)
		{
			bgfx::setTexture(MaterialTextureBundleIndex::DiffuseMap, matStageUniforms_->diffuseMap.handle, sceneFbColor_);
			renderScreenSpaceQuad(aa_ == AntiAliasing::FXAA ? fxaaFb_ : defaultFb_, ShaderProgramId::Texture, BGFX_STATE_RGB_WRITE, isTextureOriginBottomLeft_);
		}

		// FXAA.
		if (aa_ == AntiAliasing::FXAA)
		{
			bgfx::setTexture(MaterialTextureBundleIndex::DiffuseMap, matStageUniforms_->diffuseMap.handle, fxaaColor_);
			renderScreenSpaceQuad(defaultFb_, ShaderProgramId::FXAA, BGFX_STATE_RGB_WRITE, isTextureOriginBottomLeft_);
		}
	}

	dlightManager_->clear();
	sceneEntities_.clear();
	scenePolygons_.clear();
	sortedScenePolygons_.clear();
	scenePolygonVertices_.clear();
}

void Main::endFrame()
{
	flushStretchPics();
	assert(firstFreeViewId_ != 0); // Should always be one active view.

	if (debugDraw_ == DebugDraw::Depth)
	{
		debugDraw(linearDepthFb_);
	}
	else if (debugDraw_ == DebugDraw::DynamicLight)
	{
		debugDraw(dlightManager_->getTexture(), 0, 0, false);
	}
	else if (debugDraw_ == DebugDraw::Luminance && g_cvars.hdr->integer)
	{
		for (int i = 0; i < nLuminanceFrameBuffers_; i++)
		{
			debugDraw(luminanceFrameBuffers_[i], i);
		}

		debugDraw(adaptedLuminanceFB_[currentAdaptedLuminanceFB_], 0, 1);
	}

	bgfx::frame();

	if (g_cvars.wireframe->modified || g_cvars.bgfx_stats->modified || g_cvars.debugText->modified)
	{
		uint32_t debug = 0;

		if (g_cvars.wireframe->integer != 0)
			debug |= BGFX_DEBUG_WIREFRAME;

		if (g_cvars.bgfx_stats->integer != 0)
			debug |= BGFX_DEBUG_STATS;

		if (g_cvars.debugText->integer != 0)
			debug |= BGFX_DEBUG_TEXT;

		bgfx::setDebug(debug);
		g_cvars.wireframe->modified = g_cvars.bgfx_stats->modified = g_cvars.debugText->modified = qfalse;
	}

	if (g_cvars.debugDraw->modified)
	{
		debugDraw_ = DebugDrawFromString(g_cvars.debugDraw->string);
		g_cvars.debugDraw->modified = qfalse;
	}

	if (g_cvars.debugText->integer)
	{
		bgfx::dbgTextClear();
		debugTextY = 0;
	}

	firstFreeViewId_ = 0;
	frameNo_++;
	stretchPicViewId_ = UINT8_MAX;
}

bool Main::sampleLight(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir)
{
	if (!world::HasLightGrid())
		return false;

	world::SampleLightGrid(position, ambientLight, directedLight, lightDir);
	return true;
}

void Main::onMaterialCreate(Material *material)
{
	if (!util::Stricmp(material->name, "bfgExplosion"))
	{
		bfgExplosionMaterial_ = material;
	}
	else if (!util::Stricmp(material->name, "sprites/plasma1"))
	{
		plasmaBallMaterial_ = material;
	}
	else if (!util::Stricmp(material->name, "plasmaExplosion"))
	{
		plasmaExplosionMaterial_ = material;
	}
}

void Main::onModelCreate(Model *model)
{
	if (!util::Stricmp(model->getName(), "models/weaphits/bfg.md3"))
	{
		bfgMissibleModel_ = model;
	}
}

void Main::debugDraw(const FrameBuffer &texture, int x, int y, bool singleChannel)
{
	bgfx::setTexture(MaterialTextureBundleIndex::DiffuseMap, matStageUniforms_->diffuseMap.handle, texture.handle);
	renderScreenSpaceQuad(defaultFb_, singleChannel ? ShaderProgramId::TextureSingleChannel : ShaderProgramId::Texture, BGFX_STATE_RGB_WRITE, isTextureOriginBottomLeft_, Rect(g_cvars.debugDrawSize->integer * x, g_cvars.debugDrawSize->integer * y, g_cvars.debugDrawSize->integer, g_cvars.debugDrawSize->integer));
}

void Main::debugDraw(bgfx::TextureHandle texture, int x, int y, bool singleChannel)
{
	bgfx::setTexture(MaterialTextureBundleIndex::DiffuseMap, matStageUniforms_->diffuseMap.handle, texture);
	renderScreenSpaceQuad(defaultFb_, singleChannel ? ShaderProgramId::TextureSingleChannel : ShaderProgramId::Texture, BGFX_STATE_RGB_WRITE, isTextureOriginBottomLeft_, Rect(g_cvars.debugDrawSize->integer * x, g_cvars.debugDrawSize->integer * y, g_cvars.debugDrawSize->integer, g_cvars.debugDrawSize->integer));
}

uint8_t Main::pushView(const FrameBuffer &frameBuffer, uint16_t clearFlags, const mat4 &viewMatrix, const mat4 &projectionMatrix, Rect rect, int flags)
{
	// Useful for debugging, can be disabled for performance later.
#if 1
	if (firstFreeViewId_ == 0)
	{
		bgfx::setViewClear(firstFreeViewId_, clearFlags | BGFX_CLEAR_COLOR, 0xff00ffff);
	}
	else
#endif
	{
		bgfx::setViewClear(firstFreeViewId_, clearFlags);
	}

	bgfx::setViewFrameBuffer(firstFreeViewId_, frameBuffer.handle);
	bgfx::setViewRect(firstFreeViewId_, uint16_t(rect.x), uint16_t(rect.y), uint16_t(rect.w), uint16_t(rect.h));
	bgfx::setViewSeq(firstFreeViewId_, (flags & PushViewFlags::Sequential) != 0);
	bgfx::setViewTransform(firstFreeViewId_, viewMatrix.get(), projectionMatrix.get());
	firstFreeViewId_++;
	return firstFreeViewId_ - 1;
}

void Main::flushStretchPics()
{
	if (!stretchPicIndices_.empty())
	{
		bgfx::TransientVertexBuffer tvb;
		bgfx::TransientIndexBuffer tib;

		if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, (uint32_t)stretchPicVertices_.size(), &tib, (uint32_t)stretchPicIndices_.size()))
		{
			WarnOnce(WarnOnceId::TransientBuffer);
		}
		else
		{
			memcpy(tvb.data, &stretchPicVertices_[0], sizeof(Vertex) * stretchPicVertices_.size());
			memcpy(tib.data, &stretchPicIndices_[0], sizeof(uint16_t) * stretchPicIndices_.size());
			time_ = ri.Milliseconds();
			floatTime_ = time_ * 0.001f;
			matUniforms_->time.set(vec4(stretchPicMaterial_->setTime(floatTime_), 0, 0, 0));

			if (stretchPicViewId_ == UINT8_MAX)
			{
				stretchPicViewId_ = pushView(defaultFb_, BGFX_CLEAR_NONE, mat4::identity, mat4::orthographicProjection(0, (float)glConfig.vidWidth, 0, (float)glConfig.vidHeight, -1, 1), Rect(0, 0, glConfig.vidWidth, glConfig.vidHeight), PushViewFlags::Sequential);
			}

			for (const MaterialStage &stage : stretchPicMaterial_->stages)
			{
				if (!stage.active)
					continue;

				stage.setShaderUniforms(matStageUniforms_.get());
				stage.setTextureSamplers(matStageUniforms_.get());
				uint64_t state = BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | stage.getState();

				// Depth testing and writing should always be off for 2D drawing.
				state &= ~BGFX_STATE_DEPTH_TEST_MASK;
				state &= ~BGFX_STATE_DEPTH_WRITE;

				// Silence D3D11 warnings.
				bgfx::setTexture(MaterialTextureBundleIndex::DynamicLights, matStageUniforms_->textures[MaterialTextureBundleIndex::DynamicLights]->handle, g_textureCache->getWhiteTexture()->getHandle());
			
				bgfx::setState(state);
				bgfx::setVertexBuffer(&tvb);
				bgfx::setIndexBuffer(&tib);
				bgfx::submit(stretchPicViewId_, shaderPrograms_[ShaderProgramId::Generic].handle);
			}
		}
	}

	stretchPicVertices_.clear();
	stretchPicIndices_.clear();
}

static void SetDrawCallGeometry(const DrawCall &dc)
{
	assert(dc.vb.nVertices);
	assert(dc.ib.nIndices);

	if (dc.vb.type == DrawCall::BufferType::Static)
	{
		bgfx::setVertexBuffer(dc.vb.staticHandle, dc.vb.firstVertex, dc.vb.nVertices);
	}
	else if (dc.vb.type == DrawCall::BufferType::Dynamic)
	{
		bgfx::setVertexBuffer(dc.vb.dynamicHandle, dc.vb.nVertices);
	}
	else if (dc.vb.type == DrawCall::BufferType::Transient)
	{
		bgfx::setVertexBuffer(&dc.vb.transientHandle, dc.vb.firstVertex, dc.vb.nVertices);
	}

	if (dc.ib.type == DrawCall::BufferType::Static)
	{
		bgfx::setIndexBuffer(dc.ib.staticHandle, dc.ib.firstIndex, dc.ib.nIndices);
	}
	else if (dc.ib.type == DrawCall::BufferType::Dynamic)
	{
		bgfx::setIndexBuffer(dc.ib.dynamicHandle, dc.ib.firstIndex, dc.ib.nIndices);
	}
	else if (dc.ib.type == DrawCall::BufferType::Transient)
	{
		bgfx::setIndexBuffer(&dc.ib.transientHandle, dc.ib.firstIndex, dc.ib.nIndices);
	}
}

void Main::renderCamera(uint8_t visCacheId, vec3 pvsPosition, vec3 position, mat3 rotation, Rect rect, vec2 fov, const uint8_t *areaMask)
{
	assert(areaMask);
	const float zMin = 4;
	float zMax = 2048;
	const float polygonDepthOffset = -0.001f;
	const bool isMainCamera = visCacheId == mainVisCacheId_;

	if (isWorldCamera_)
	{
		world::UpdateVisCache(visCacheId, pvsPosition, areaMask);

		// Use dynamic z max.
		zMax = world::GetBounds(visCacheId).calculateFarthestCornerDistance(position);
	}

	const mat4 viewMatrix = toOpenGlMatrix_ * mat4::view(position, rotation);
	const mat4 projectionMatrix = mat4::perspectiveProjection(fov.x, fov.y, zMin, zMax);
	const mat4 vpMatrix(projectionMatrix * viewMatrix);

	if (isWorldCamera_ && isMainCamera)
	{
		for (size_t i = 0; i < world::GetNumPortalSurfaces(visCacheId); i++)
		{
			vec3 pvsPosition;
			Transform portalCamera;

			if (world::CalculatePortalCamera(visCacheId, i, position, rotation, vpMatrix, sceneEntities_, &pvsPosition, &portalCamera, &isMirrorCamera_, &portalPlane_))
			{
				renderCamera(portalVisCacheId_, pvsPosition, portalCamera.position, portalCamera.rotation, rect, fov, areaMask);
				isMirrorCamera_ = false;
				break;
			}
		}
	}

	cameraFrustum_ = Frustum(vpMatrix);

	// Build draw calls. Order doesn't matter.
	drawCalls_.clear();

	if (isWorldCamera_)
	{
		Sky_Render(&drawCalls_, position, visCacheId, zMax);
		world::Render(&drawCalls_, visCacheId);
	}

	for (auto &entity : sceneEntities_)
	{
		if (isMainCamera && (entity.e.renderfx & RF_THIRD_PERSON) != 0)
			continue;

		if (!isMainCamera && (entity.e.renderfx & RF_FIRST_PERSON) != 0)
			continue;

		renderEntity(&drawCalls_, position, rotation, &entity);
	}

	if (!scenePolygons_.empty())
	{
		// Sort polygons by material and fogIndex for batching.
		for (auto &polygon : scenePolygons_)
		{
			sortedScenePolygons_.push_back(&polygon);
		}

		std::sort(sortedScenePolygons_.begin(), sortedScenePolygons_.end(), [](Polygon *a, Polygon *b)
		{
			if (a->material->index < b->material->index)
				return true;
			else if (a->material->index == b->material->index)
			{
				return a->fogIndex < b->fogIndex;
			}

			return false;
		});

		size_t batchStart = 0;
		
		for (;;)
		{
			uint32_t nVertices = 0, nIndices = 0;
			size_t batchEnd;

			// Find the last polygon index that matches the current material and fog. Count geo as we go.
			for (batchEnd = batchStart; batchEnd < sortedScenePolygons_.size(); batchEnd++)
			{
				const Polygon *p = sortedScenePolygons_[batchEnd];

				if (p->material != sortedScenePolygons_[batchStart]->material || p->fogIndex != sortedScenePolygons_[batchStart]->fogIndex)
					break;
				
				nVertices += p->nVertices;
				nIndices += (p->nVertices - 2) * 3;
			}

			batchEnd = std::max(batchStart, batchEnd - 1);

			// Got a range of polygons to batch. Build a draw call.
			bgfx::TransientVertexBuffer tvb;
			bgfx::TransientIndexBuffer tib;

			if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, nVertices, &tib, nIndices) )
			{
				WarnOnce(WarnOnceId::TransientBuffer);
				break;
			}

			auto vertices = (Vertex *)tvb.data;
			auto indices = (uint16_t *)tib.data;
			uint32_t currentVertex = 0, currentIndex = 0;

			for (size_t i = batchStart; i <= batchEnd; i++)
			{
				const Polygon *p = sortedScenePolygons_[i];
				const uint32_t firstVertex = currentVertex;

				for (size_t j = 0; j < p->nVertices; j++)
				{
					auto &v = vertices[currentVertex++];
					auto &pv = scenePolygonVertices_[p->firstVertex + j];
					v.pos = pv.xyz;
					v.texCoord = pv.st;
					v.color = vec4::fromBytes(pv.modulate);
				}

				for (size_t j = 0; j < p->nVertices - 2; j++)
				{
					indices[currentIndex++] = firstVertex + 0;
					indices[currentIndex++] = firstVertex + uint16_t(j) + 1;
					indices[currentIndex++] = firstVertex + uint16_t(j) + 2;
				}
			}

			DrawCall dc;
			dc.fogIndex = sortedScenePolygons_[batchStart]->fogIndex;
			dc.material = sortedScenePolygons_[batchStart]->material;
			dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
			dc.vb.transientHandle = tvb;
			dc.vb.nVertices = nVertices;
			dc.ib.transientHandle = tib;
			dc.ib.nIndices = nIndices;
			drawCalls_.push_back(dc);

			// Iterate.
			batchStart = batchEnd + 1;

			if (batchStart >= sortedScenePolygons_.size())
				break;
		}
	}

	if (drawCalls_.empty())
		return;

	// Do material CPU deforms.
	for (DrawCall &dc : drawCalls_)
	{
		assert(dc.material);

		if (dc.material->hasCpuDeforms())
		{
			// Material requires CPU deforms, but geometry isn't available in system memory.
			if (dc.vb.type != DrawCall::BufferType::Transient || dc.ib.type != DrawCall::BufferType::Transient)
				continue;

			currentEntity_ = dc.entity;
			dc.material->setTime(floatTime_);
			dc.material->doCpuDeforms(&dc, sceneRotation_);
			currentEntity_ = nullptr;
		}
	}

	// Sort draw calls.
	std::sort(drawCalls_.begin(), drawCalls_.end());

	// Set portal clipping.
	if (!isMainCamera)
	{
		uniforms_->portalClip.set(vec4(1, 0, 0, 0));
		uniforms_->portalPlane.set(portalPlane_);
	}
	else
	{
		uniforms_->portalClip.set(vec4(0, 0, 0, 0));
	}

	// Setup dynamic lights.
	if (isWorldCamera_)
	{
		dlightManager_->update(frameNo_, &uniforms_->dynamicLights_Num_TextureWidth);
	}
	else
	{
		// For non-world scenes, dlight contribution is added to entities in setupEntityLighting, so write 0 to the uniform for num dlights.
		uniforms_->dynamicLights_Num_TextureWidth.set(vec4::empty);
	}

	// Render depth.
	if (isWorldCamera_)
	{
		const uint8_t viewId = pushView(sceneFb_, BGFX_CLEAR_DEPTH, viewMatrix, projectionMatrix, rect);

		for (DrawCall &dc : drawCalls_)
		{
			// Material remapping.
			auto mat = dc.material->remappedShader ? dc.material->remappedShader : dc.material;

			if (mat->sort != MaterialSort::Opaque || mat->numUnfoggedPasses == 0)
				continue;

			currentEntity_ = dc.entity;
			matUniforms_->time.set(vec4(mat->setTime(floatTime_), 0, 0, 0));
			const mat4 modelViewMatrix(viewMatrix * dc.modelMatrix);
			uniforms_->depthRange.set(vec4(dc.zOffset, dc.zScale, zMin, zMax));
			mat->setDeformUniforms(matUniforms_.get());

			// See if any of the stages use alpha testing.
			const MaterialStage *alphaTestStage = nullptr;

			for (const MaterialStage &stage : mat->stages)
			{
				if (stage.active && stage.alphaTest != MaterialAlphaTest::None)
				{
					alphaTestStage = &stage;
					break;
				}
			}

			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			uint64_t state = BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_DEPTH_WRITE;

			// Grab the cull state. Doesn't matter which stage, since it's global to the material.
			state |= mat->stages[0].getState() & BGFX_STATE_CULL_MASK;

			if (alphaTestStage)
			{
				alphaTestStage->setShaderUniforms(matStageUniforms_.get(), MaterialStageSetUniformsFlags::TexGen);
				alphaTestStage->setTextureSamplers(matStageUniforms_.get());
				bgfx::setState(state);
				bgfx::submit(viewId, shaderPrograms_[ShaderProgramId::Depth_AlphaTest].handle);
			}
			else
			{
				matStageUniforms_->alphaTest.set(vec4::empty);
				bgfx::setState(state);
				bgfx::submit(viewId, shaderPrograms_[ShaderProgramId::Depth].handle);
			}

			currentEntity_ = nullptr;
		}

		// Read depth, write linear depth.
		uniforms_->depthRange.set(vec4(0, 0, zMin, zMax));
		bgfx::setTexture(MaterialTextureBundleIndex::Depth, matStageUniforms_->depthSampler.handle, sceneFbDepth_);
		renderScreenSpaceQuad(linearDepthFb_, ShaderProgramId::LinearDepth, BGFX_STATE_RGB_WRITE, isTextureOriginBottomLeft_);
	}

	uint8_t mainViewId;
	
	if (isWorldCamera_)
	{
		mainViewId = pushView(sceneFb_, BGFX_CLEAR_NONE, viewMatrix, projectionMatrix, rect, PushViewFlags::Sequential);
	}
	else
	{
		mainViewId = pushView(defaultFb_, BGFX_CLEAR_DEPTH, viewMatrix, projectionMatrix, rect, PushViewFlags::Sequential);
	}

	for (DrawCall &dc : drawCalls_)
	{
		assert(dc.material);

		// Material remapping.
		auto mat = dc.material->remappedShader ? dc.material->remappedShader : dc.material;

		// Special case for skybox.
		if (dc.flags >= DrawCallFlags::SkyboxSideFirst && dc.flags <= DrawCallFlags::SkyboxSideLast)
		{
			uniforms_->depthRange.set(vec4(dc.zOffset, dc.zScale, zMin, zMax));
			matStageUniforms_->alphaTest.set(vec4::empty);
			matStageUniforms_->baseColor.set(vec4::white);
			matStageUniforms_->generators.set(vec4::empty);
			matStageUniforms_->lightType.set(vec4::empty);
			matStageUniforms_->vertexColor.set(vec4::black);
			const int sky_texorder[6] = { 0, 2, 1, 3, 4, 5 };
			const int side = dc.flags - DrawCallFlags::SkyboxSideFirst;
			bgfx::setTexture(MaterialTextureBundleIndex::DiffuseMap, matStageUniforms_->diffuseMap.handle, mat->sky.outerbox[sky_texorder[side]]->getHandle());
			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			bgfx::setState(dc.state);
			bgfx::submit(mainViewId, shaderPrograms_[ShaderProgramId::Generic].handle);
			continue;
		}

		const bool doFogPass = dc.fogIndex >= 0 && mat->fogPass != MaterialFogPass::None;

		if (mat->numUnfoggedPasses == 0 && !doFogPass)
			continue;

		currentEntity_ = dc.entity;
		matUniforms_->time.set(vec4(mat->setTime(floatTime_), 0, 0, 0));
		const mat4 modelViewMatrix(viewMatrix * dc.modelMatrix);

		if (mat->polygonOffset)
		{
			uniforms_->depthRange.set(vec4(dc.zOffset + polygonDepthOffset, dc.zScale, zMin, zMax));
		}
		else
		{
			uniforms_->depthRange.set(vec4(dc.zOffset, dc.zScale, zMin, zMax));
		}

		uniforms_->viewOrigin.set(position);
		mat->setDeformUniforms(matUniforms_.get());
		const vec3 localViewPosition = currentEntity_ ? currentEntity_->localViewPosition : position;
		uniforms_->localViewOrigin.set(localViewPosition);

		if (currentEntity_)
		{
			entityUniforms_->ambientLight.set(vec4(currentEntity_->ambientLight / 255.0f, 0));
			entityUniforms_->directedLight.set(vec4(currentEntity_->directedLight / 255.0f, 0));
			entityUniforms_->lightDirection.set(vec4(currentEntity_->lightDir, 0));
			entityUniforms_->modelLightDir.set(currentEntity_->modelLightDir);
		}

		vec4 fogColor, fogDistance, fogDepth;
		float eyeT;

		if (dc.fogIndex >= 0)
		{
			world::CalculateFog(dc.fogIndex, dc.modelMatrix, modelViewMatrix, position, localViewPosition, rotation, &fogColor, &fogDistance, &fogDepth, &eyeT);
			uniforms_->fogDistance.set(fogDistance);
			uniforms_->fogDepth.set(fogDepth);
			uniforms_->fogEyeT.set(eyeT);
		}

		for (const MaterialStage &stage : mat->stages)
		{
			if (!stage.active)
				continue;

			if (dc.fogIndex >= 0 && stage.adjustColorsForFog != MaterialAdjustColorsForFog::None)
			{
				uniforms_->fogEnabled.set(vec4(1, 0, 0, 0));
				matStageUniforms_->fogColorMask.set(stage.getFogColorMask());
			}
			else
			{
				uniforms_->fogEnabled.set(vec4::empty);
			}

			stage.setShaderUniforms(matStageUniforms_.get());
			stage.setTextureSamplers(matStageUniforms_.get());
			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			ShaderProgramId::Enum shaderProgram;
			uint64_t state = dc.state | stage.getState();

			if (SETTINGS_SOFT_SPRITES && dc.softSpriteDepth > 0)
			{
				if (stage.alphaTest != MaterialAlphaTest::None)
				{
					shaderProgram = ShaderProgramId::Generic_AlphaTestSoftSprite;
				}
				else
				{
					shaderProgram = ShaderProgramId::Generic_SoftSprite;
				}

				bgfx::setTexture(MaterialTextureBundleIndex::Depth, matStageUniforms_->textures[MaterialTextureBundleIndex::Depth]->handle, linearDepthFb_.handle);
				
				// Change additive blend from (1, 1) to (src alpha, 1) so the soft sprite shader can control alpha.
				float useAlpha = 1;

				if ((state & BGFX_STATE_BLEND_MASK) == BGFX_STATE_BLEND_ADD)
				{
					useAlpha = 0; // Ignore existing alpha values in the shader. This preserves the behavior of a (1, 1) additive blend.
					state &= ~BGFX_STATE_BLEND_MASK;
					state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
				}

				uniforms_->softSprite_Depth_UseAlpha.set(vec4(dc.softSpriteDepth, useAlpha, 0, 0));
			}
			else if (stage.alphaTest != MaterialAlphaTest::None)
			{
				shaderProgram = ShaderProgramId::Generic_AlphaTest;
			}
			else
			{
				shaderProgram = ShaderProgramId::Generic;
			}

			if (isWorldCamera_)
			{
				bgfx::setTexture(MaterialTextureBundleIndex::DynamicLights, matStageUniforms_->textures[MaterialTextureBundleIndex::DynamicLights]->handle, dlightManager_->getTexture());
			}
			else
			{
				// Silence D3D11 warnings.
				bgfx::setTexture(MaterialTextureBundleIndex::DynamicLights, matStageUniforms_->textures[MaterialTextureBundleIndex::DynamicLights]->handle, g_textureCache->getWhiteTexture()->getHandle());
			}

			bgfx::setState(state);
			bgfx::submit(mainViewId, shaderPrograms_[shaderProgram].handle);
		}

		// Do fog pass.
		if (doFogPass)
		{
			matStageUniforms_->color.set(fogColor);
			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			uint64_t state = dc.state | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);

			if (mat->fogPass == MaterialFogPass::Equal)
			{
				state |= BGFX_STATE_DEPTH_TEST_EQUAL;
			}
			else
			{
				state |= BGFX_STATE_DEPTH_TEST_LEQUAL;
			}

			bgfx::setState(state);
			bgfx::submit(mainViewId, shaderPrograms_[ShaderProgramId::Fog].handle);
		}

		currentEntity_ = nullptr;
	}
}

// From bgfx screenSpaceQuad.
void Main::renderScreenSpaceQuad(const FrameBuffer &frameBuffer, ShaderProgramId::Enum program, uint64_t state, bool originBottomLeft, Rect rect)
{
	if (!bgfx::checkAvailTransientVertexBuffer(3, Vertex::decl))
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	if (!rect.w) rect.w = glConfig.vidWidth;
	if (!rect.h) rect.h = glConfig.vidHeight;
	const float width = 1.0f;
	const float height = 1.0f;
	const float zz = 0.0f;
	const float minx = -width;
	const float maxx =  width;
	const float miny = 0.0f;
	const float maxy = height*2.0f;
	const float texelHalfW = halfTexelOffset_ / rect.w;
	const float texelHalfH = halfTexelOffset_ / rect.h;
	const float minu = -1.0f + texelHalfW;
	const float maxu =  1.0f + texelHalfW;
	float minv = texelHalfH;
	float maxv = 2.0f + texelHalfH;

	if (originBottomLeft)
	{
		float temp = minv;
		minv = maxv;
		maxv = temp;
		minv -= 1.0f;
		maxv -= 1.0f;
	}

	bgfx::TransientVertexBuffer vb;
	bgfx::allocTransientVertexBuffer(&vb, 3, Vertex::decl);
	auto vertices = (Vertex *)vb.data;
	vertices[0].pos = vec3(minx, miny, zz);
	vertices[0].color = vec4::white;
	vertices[0].texCoord = vec2(minu, minv);
	vertices[1].pos = vec3(maxx, miny, zz);
	vertices[1].color = vec4::white;
	vertices[1].texCoord = vec2(maxu, minv);
	vertices[2].pos = vec3(maxx, maxy, zz);
	vertices[2].color = vec4::white;
	vertices[2].texCoord = vec2(maxu, maxv);
	bgfx::setVertexBuffer(&vb);
	bgfx::setState(state);
	const uint8_t viewId = pushView(frameBuffer, BGFX_CLEAR_NONE, mat4::identity, mat4::orthographicProjection(0, 1, 0, 1, -1, 1), rect);
	bgfx::submit(viewId, shaderPrograms_[program].handle);
}

// From bgfx HDR example setOffsets2x2Lum
void Main::setTexelOffsetsDownsample2x2(int width, int height)
{
	const float du = 1.0f / width;
	const float dv = 1.0f / height;
	vec4 offsets[16];
	uint32_t num = 0;

	for (uint32_t yy = 0; yy < 3; ++yy)
	{
		for (uint32_t xx = 0; xx < 3; ++xx)
		{
			offsets[num][0] = (xx - halfTexelOffset_) * du;
			offsets[num][1] = (yy - halfTexelOffset_) * dv;
			++num;
		}
	}

	uniforms_->texelOffsets.set(offsets, num);
}

// From bgfx HDR example setOffsets4x4Lum
void Main::setTexelOffsetsDownsample4x4(int width, int height)
{
	const float du = 1.0f / width;
	const float dv = 1.0f / height;
	vec4 offsets[16];
	uint32_t num = 0;

	for (uint32_t yy = 0; yy < 4; ++yy)
	{
		for (uint32_t xx = 0; xx < 4; ++xx)
		{
			offsets[num][0] = (xx - 1.0f - halfTexelOffset_) * du;
			offsets[num][1] = (yy - 1.0f - halfTexelOffset_) * dv;
			++num;
		}
	}

	uniforms_->texelOffsets.set(offsets, num);
}

void Main::renderEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity)
{
	assert(drawCallList);
	assert(entity);

	// Calculate the viewer origin in the model's space.
	// Needed for fog, specular, and environment mapping.
	const vec3 delta = viewPosition - vec3(entity->e.origin);

	// Compensate for scale in the axes if necessary.
	float axisLength = 1;

	if (entity->e.nonNormalizedAxes)
	{
		axisLength = 1.0f / vec3(entity->e.axis[0]).length();
	}

	entity->localViewPosition =
	{
		vec3::dotProduct(delta, entity->e.axis[0]) * axisLength,
		vec3::dotProduct(delta, entity->e.axis[1]) * axisLength,
		vec3::dotProduct(delta, entity->e.axis[2]) * axisLength
	};

	switch (entity->e.reType)
	{
	case RT_BEAM:
		break;

	case RT_LIGHTNING:
		renderLightningEntity(drawCallList, viewPosition, viewRotation, entity);
		break;

	case RT_MODEL:
		{
			auto model = modelCache_->getModel(entity->e.hModel);

			if (model->isCulled(entity, cameraFrustum_))
				break;

			setupEntityLighting(entity);
			model->render(drawCallList, entity);
		}
		break;
	
	case RT_RAIL_CORE:
		renderRailCoreEntity(drawCallList, viewPosition, viewRotation, entity);
		break;

	case RT_RAIL_RINGS:
		renderRailRingsEntity(drawCallList, entity);
		break;

	case RT_SPRITE:
		if (cameraFrustum_.clipSphere(entity->e.origin, entity->e.radius) == Frustum::ClipResult::Outside)
			break;

		renderSpriteEntity(drawCallList, viewRotation, entity);
		break;

	default:
		break;
	}
}

void Main::renderLightningEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity)
{
	assert(drawCallList);
	const vec3 start(entity->e.origin), end(entity->e.oldorigin);
	vec3 dir = (end - start);
	const float length = dir.normalize();

	// Compute side vector.
	const vec3 v1 = (start - viewPosition).normal();
	const vec3 v2 = (end - viewPosition).normal();
	vec3 right = vec3::crossProduct(v1, v2).normal();

	for (int i = 0; i < 4; i++)
	{
		renderRailCore(drawCallList, start, end, right, length, g_cvars.railCoreWidth->value, materialCache_->getMaterial(entity->e.customShader), vec4::fromBytes(entity->e.shaderRGBA), entity);
		right = right.rotatedAroundDirection(dir, 45);
	}
}

void Main::renderRailCoreEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity)
{
	assert(drawCallList);
	const vec3 start(entity->e.oldorigin), end(entity->e.origin);
	vec3 dir = (end - start);
	const float length = dir.normalize();

	// Compute side vector.
	const vec3 v1 = (start - viewPosition).normal();
	const vec3 v2 = (end - viewPosition).normal();
	const vec3 right = vec3::crossProduct(v1, v2).normal();

	renderRailCore(drawCallList, start, end, right, length, g_cvars.railCoreWidth->value, materialCache_->getMaterial(entity->e.customShader), vec4::fromBytes(entity->e.shaderRGBA), entity);
}

void Main::renderRailCore(DrawCallList *drawCallList, vec3 start, vec3 end, vec3 up, float length, float spanWidth, Material *mat, vec4 color, Entity *entity)
{
	assert(drawCallList);
	const uint32_t nVertices = 4, nIndices = 6;
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, nVertices, &tib, nIndices)) 
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	auto vertices = (Vertex *)tvb.data;
	vertices[0].pos = start + up * spanWidth;
	vertices[1].pos = start + up * -spanWidth;
	vertices[2].pos = end + up * spanWidth;
	vertices[3].pos = end + up * -spanWidth;

	const float t = length / 256.0f;
	vertices[0].texCoord = vec2(0, 0);
	vertices[1].texCoord = vec2(0, 1);
	vertices[2].texCoord = vec2(t, 0);
	vertices[3].texCoord = vec2(t, 1);

	vertices[0].color = vec4(color.xyz() * 0.25f, 1);
	vertices[1].color = vertices[2].color = vertices[3].color = color;

	auto indices = (uint16_t *)tib.data;
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 2; indices[4] = 1; indices[5] = 3;

	DrawCall dc;
	dc.entity = entity;
	dc.fogIndex = isWorldCamera_ ? world::FindFogIndex(entity->e.origin, entity->e.radius) : -1;
	dc.material = mat;
	dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
	dc.vb.transientHandle = tvb;
	dc.vb.nVertices = nVertices;
	dc.ib.transientHandle = tib;
	dc.ib.nIndices = nIndices;
	drawCallList->push_back(dc);
}

void Main::renderRailRingsEntity(DrawCallList *drawCallList, Entity *entity)
{
	assert(drawCallList);
	const vec3 start(entity->e.oldorigin), end(entity->e.origin);
	vec3 dir = (end - start);
	const float length = dir.normalize();
	vec3 right, up;
	dir.toNormalVectors(&right, &up);
	dir *= g_cvars.railSegmentLength->value;
	int nSegments = (int)std::max(1.0f, length / g_cvars.railSegmentLength->value);

	if (nSegments > 1)
		nSegments--;

	if (!nSegments)
		return;

	const float scale = 0.25f;
	const float spanWidth = g_cvars.railWidth->value;
	vec3 positions[4];

	for (int i = 0; i < 4; i++)
	{
		const float c = cos(DEG2RAD(45 + i * 90));
		const float s = sin(DEG2RAD(45 + i * 90));
		positions[i] = start + (right * c + up * s) * scale * spanWidth;

		if (nSegments)
		{
			// Offset by 1 segment if we're doing a long distance shot.
			positions[i] += dir;
		}
	}

	const uint32_t nVertices = 4 * nSegments, nIndices = 6 * nSegments;
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, nVertices, &tib, nIndices)) 
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	for (int i = 0; i < nSegments; i++)
	{
		for (int j = 0; j < 4; j++ )
		{
			auto vertex = &((Vertex *)tvb.data)[i * 4 + j];
			vertex->pos = positions[j];
			vertex->texCoord[0] = j < 2;
			vertex->texCoord[1] = j && j != 3;
			vertex->color = vec4::fromBytes(entity->e.shaderRGBA);
			positions[j] += dir;
		}

		auto index = &((uint16_t *)tib.data)[i * 6];
		const uint16_t offset = i * 4;
		index[0] = offset + 0; index[1] = offset + 1; index[2] = offset + 3;
		index[3] = offset + 3; index[4] = offset + 1; index[5] = offset + 2;
	}

	DrawCall dc;
	dc.entity = entity;
	dc.fogIndex = isWorldCamera_ ? world::FindFogIndex(entity->e.origin, entity->e.radius) : -1;
	dc.material = materialCache_->getMaterial(entity->e.customShader);
	dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
	dc.vb.transientHandle = tvb;
	dc.vb.nVertices = nVertices;
	dc.ib.transientHandle = tib;
	dc.ib.nIndices = nIndices;
	drawCallList->push_back(dc);
}

void Main::renderSpriteEntity(DrawCallList *drawCallList, mat3 viewRotation, Entity *entity)
{
	assert(drawCallList);

	// Calculate the positions for the four corners.
	vec3 left, up;

	if (entity->e.rotation == 0)
	{
		left = viewRotation[1] * entity->e.radius;
		up = viewRotation[2] * entity->e.radius;
	}
	else
	{
		const float ang = (float)M_PI * entity->e.rotation / 180.0f;
		const float s = sin(ang);
		const float c = cos(ang);
		left = viewRotation[1] * (c * entity->e.radius) + viewRotation[2] * (-s * entity->e.radius);
		up = viewRotation[2] * (c * entity->e.radius) + viewRotation[1] * (s * entity->e.radius);
	}

	if (isMirrorCamera_)
		left = -left;

	const uint32_t nVertices = 4, nIndices = 6;
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, nVertices, &tib, nIndices)) 
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	auto vertices = (Vertex *)tvb.data;
	const vec3 position(entity->e.origin);
	vertices[0].pos = position + left + up;
	vertices[1].pos = position - left + up;
	vertices[2].pos = position - left - up;
	vertices[3].pos = position + left - up;

	// Constant normal all the way around.
	vertices[0].normal = vertices[1].normal = vertices[2].normal = vertices[3].normal = -viewRotation[0];

	// Standard square texture coordinates.
	vertices[0].texCoord = vertices[0].texCoord2 = vec2(0, 0);
	vertices[1].texCoord = vertices[1].texCoord2 = vec2(1, 0);
	vertices[2].texCoord = vertices[2].texCoord2 = vec2(1, 1);
	vertices[3].texCoord = vertices[3].texCoord2 = vec2(0, 1);

	// Constant color all the way around.
	vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = vec4::fromBytes(entity->e.shaderRGBA);

	auto indices = (uint16_t *)tib.data;
	indices[0] = 0; indices[1] = 1; indices[2] = 3;
	indices[3] = 3; indices[4] = 1; indices[5] = 2;

	DrawCall dc;
	dc.entity = entity;
	dc.fogIndex = isWorldCamera_ ? world::FindFogIndex(entity->e.origin, entity->e.radius) : -1;
	dc.material = materialCache_->getMaterial(entity->e.customShader);
	dc.softSpriteDepth = entity->e.radius / 2.0f;
	dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
	dc.vb.transientHandle = tvb;
	dc.vb.nVertices = nVertices;
	dc.ib.transientHandle = tib;
	dc.ib.nIndices = nIndices;
	drawCallList->push_back(dc);
}

void Main::setupEntityLighting(Entity *entity)
{
	assert(entity);

	// Trace a sample point down to find ambient light.
	vec3 lightPosition;
	
	if (entity->e.renderfx & RF_LIGHTING_ORIGIN)
	{
		// Seperate lightOrigins are needed so an object that is sinking into the ground can still be lit, and so multi-part models can be lit identically.
		lightPosition = entity->e.lightingOrigin;
	}
	else
	{
		lightPosition = entity->e.origin;
	}

	// If not a world model, only use dynamic lights (menu system, etc.)
	if (isWorldCamera_ && world::HasLightGrid())
	{
		world::SampleLightGrid(lightPosition, &entity->ambientLight, &entity->directedLight, &entity->lightDir);
	}
	else
	{
		entity->ambientLight = vec3(g_identityLight * 150);
		entity->directedLight = vec3(g_identityLight * 150);
		entity->lightDir = sunLight_.direction;
	}

	vec3 lightDir = entity->lightDir * entity->directedLight.length();

	// Bonus items and view weapons have a fixed minimum add.
	//if (entity->e.renderfx & RF_MINLIGHT)
	{
		// Give everything a minimum light add.
		entity->ambientLight += vec3(g_identityLight * 32);
	}

	// Modify the light by dynamic lights.
	if (!isWorldCamera_)
	{
		dlightManager_->contribute(frameNo_, lightPosition, &entity->directedLight, &lightDir);
	}

	entity->lightDir = lightDir.normal();

	// Clamp ambient.
	for (size_t i = 0; i < 3; i++)
	{
		entity->ambientLight[i] = std::min(entity->ambientLight[i], g_identityLight * 255);
	}

	// Transform the direction to local space.
	entity->modelLightDir[0] = vec3::dotProduct(entity->lightDir, entity->e.axis[0]);
	entity->modelLightDir[1] = vec3::dotProduct(entity->lightDir, entity->e.axis[1]);
	entity->modelLightDir[2] = vec3::dotProduct(entity->lightDir, entity->e.axis[2]);
}

float Main::calculateExplosionLight(float entityShaderTime, float durationMilliseconds) const
{
	// From CG_AddExplosion
	float light = (floatTime_ - entityShaderTime) / (durationMilliseconds / 1000.0f);

	if (light < 0.5f)
		return 1.0f;

	return 1.0f - (light - 0.5f) * 2.0f;
}

DebugDraw DebugDrawFromString(const char *s)
{
	if (util::Stricmp(s, "depth") == 0)
		return DebugDraw::Depth;
	else if (util::Stricmp(s, "dlight") == 0)
		return DebugDraw::DynamicLight;
	else if (util::Stricmp(s, "lum") == 0)
		return DebugDraw::Luminance;

	return DebugDraw::None;
}

} // namespace renderer
