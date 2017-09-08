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
/***********************************************************
* A single header file OpenGL lightmapping library         *
* https://github.com/ands/lightmapper                      *
* no warranty implied | use at your own risk               *
* author: Andreas Mantler (ands) | last change: 12.06.2016 *
*                                                          *
* License:                                                 *
* This software is in the public domain.                   *
* Where that dedication is not recognized,                 *
* you are granted a perpetual, irrevocable license to copy *
* and modify this file however you want.                   *
***********************************************************/
#include "Precompiled.h"
#pragma hdrstop

#if defined(USE_LIGHT_BAKER)
#include "LightBaker.h"
#include "Main.h" // need to access main internals for hemicube integration

#include "stb_image_write.h"

namespace renderer {
namespace light_baker {

// optional: set material characteristics by specifying cos(theta)-dependent weights for incoming light.
typedef float (*lm_weight_func)(float cos_theta, void *userdata);

static float lm_defaultWeights(float cos_theta, void *userdata)
{
	return 1.0f;
}

void InitializeIndirectLight()
{
	// Create framebuffers and textures to read from. Only done once (persistent).
	if (!bgfx::isValid(s_lightBakerPersistent->hemicubeFb[0].handle))
	{
		// Render directly into this. Ping-pong between this and the second hemicube FB when downsampling.
		bgfx::TextureHandle hemicubeTextures[2];
		hemicubeTextures[0] = bgfx::createTexture2D(s_lightBaker->hemicubeBatchSize.x, s_lightBaker->hemicubeBatchSize.y, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_RT | BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP);
		hemicubeTextures[1] = bgfx::createTexture2D(s_lightBaker->hemicubeBatchSize.x, s_lightBaker->hemicubeBatchSize.y, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT);
		s_lightBakerPersistent->hemicubeFb[0].handle = bgfx::createFrameBuffer(2, hemicubeTextures, true);

		// Downsampling.
		s_lightBakerPersistent->hemicubeFb[1].handle = bgfx::createFrameBuffer(s_lightBaker->hemicubeDownsampleSize.x, s_lightBaker->hemicubeDownsampleSize.y, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_RT | BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP);

		// Textures to read from.
#ifdef DEBUG_HEMICUBE_RENDERING
		s_lightBakerPersistent->hemicubeBatchReadTexture = bgfx::createTexture2D(s_lightBaker->hemicubeBatchSize.x, s_lightBaker->hemicubeBatchSize.y, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
		s_lightBakerPersistent->hemicubeWeightedBatchReadTexture = bgfx::createTexture2D(s_lightBaker->hemicubeDownsampleSize.x, s_lightBaker->hemicubeDownsampleSize.y, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
#endif
		s_lightBakerPersistent->hemicubeIntegrationReadTexture = bgfx::createTexture2D(s_lightBaker->nHemicubesInBatch.x, s_lightBaker->nHemicubesInBatch.y, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
	}

	if (s_lightBaker->currentIndirectBounce == 0)
	{
		// hemisphere weights texture. bakes in material dependent attenuation behaviour.
		// precalculate weights for incoming light depending on its angle. (default: all weights are 1.0f)
		lm_weight_func f = lm_defaultWeights;
		void *userdata = nullptr;
		const bgfx::Memory *weightsMem = bgfx::alloc(2 * 3 * s_lightBaker->hemicubeFaceSize * s_lightBaker->hemicubeFaceSize * sizeof(float));
		memset(weightsMem->data, 0, weightsMem->size);
		auto weights = (float *)weightsMem->data;
		float center = (s_lightBaker->hemicubeFaceSize - 1) * 0.5f;
		double sum = 0.0;
		for (int y = 0; y < s_lightBaker->hemicubeFaceSize; y++)
		{
			float dy = 2.0f * (y - center) / (float)s_lightBaker->hemicubeFaceSize;
			for (int x = 0; x < s_lightBaker->hemicubeFaceSize; x++)
			{
				float dx = 2.0f * (x - center) / (float)s_lightBaker->hemicubeFaceSize;
				vec3 v = vec3(dx, dy, 1.0f).normal();

				float solidAngle = v.z * v.z * v.z;

				float *w0 = weights + 2 * (y * (3 * s_lightBaker->hemicubeFaceSize) + x);
				float *w1 = w0 + 2 * s_lightBaker->hemicubeFaceSize;
				float *w2 = w1 + 2 * s_lightBaker->hemicubeFaceSize;

				// center weights
				w0[0] = solidAngle * f(v.z, userdata);
				w0[1] = solidAngle;

				// left/right side weights
				w1[0] = solidAngle * f(fabs(v.x), userdata);
				w1[1] = solidAngle;

				// up/down side weights
				w2[0] = solidAngle * f(fabs(v.y), userdata);
				w2[1] = solidAngle;

				sum += 3.0 * (double)solidAngle;
			}
		}

		// normalize weights
		float weightScale = (float)(1.0 / sum);
		for (int i = 0; i < 2 * s_lightBaker->hemicubeSize.x * s_lightBaker->hemicubeSize.y; i++)
			weights[i] *= weightScale;

		// upload weight texture
		Image image;
		image.width = s_lightBaker->hemicubeSize.x;
		image.height = s_lightBaker->hemicubeSize.y;
		image.data = (uint8_t *)weightsMem;
		image.flags = ImageFlags::DataIsBgfxMemory;
		s_lightBaker->hemicubeWeightsTexture = g_textureCache->create("*hemicube_weights", image, 0, bgfx::TextureFormat::RG32F);

		// Create lightmap textures to use when rendering hemicubes. Initialized with direct light.
		s_lightBaker->hemicubeLightmaps.resize(world::GetNumLightmaps());

		for (int i = 0; i < world::GetNumLightmaps(); i++)
		{
			const vec2i lightmapSize = world::GetLightmapSize();
			Image image;
			image.width = lightmapSize.x;
			image.height = lightmapSize.y;
			image.nComponents = 4;
			image.dataSize = image.width * image.height * image.nComponents;
			image.data = &s_lightBaker->lightmaps[i].encodedPassColor[0].x;
			s_lightBaker->hemicubeLightmaps[i] = g_textureCache->create(util::VarArgs("*hemicube_lightmap%d", (int)i), image, TextureFlags::ClampToEdge | TextureFlags::Mutable);
		}

		// Misc. state.
	#ifdef DEBUG_HEMICUBE_RENDERING
		s_lightBaker->hemicubeBatchData.resize(s_lightBaker->hemicubeBatchSize.x * s_lightBaker->hemicubeBatchSize.y * 4);
		s_lightBaker->hemicubeWeightedBatchData.resize(s_lightBaker->hemicubeDownsampleSize.x * s_lightBaker->hemicubeDownsampleSize.y * 4);
	#endif
		s_lightBaker->hemicubeIntegrationData.resize(s_lightBaker->nHemicubesInBatch.x * s_lightBaker->nHemicubesInBatch.y * 4);
		s_lightBaker->hemicubeBatchLocations.resize(s_lightBaker->nHemicubesInBatch.x * s_lightBaker->nHemicubesInBatch.y);
	}

	s_lightBaker->indirectLightProgress = 0;
	s_lightBaker->nHemicubesToRenderPerFrame = 8;
	s_lightBaker->nHemicubesRenderedInBatch = 0;
	s_lightBaker->nHemicubeBatchesProcessed = 0;
	s_lightBaker->finishedHemicubeBatch = false;
	s_lightBaker->finishedBakingIndirect = false;
	InitializeRasterization(2, 0.01f);
}

static uint32_t AsyncReadTexture(bgfx::FrameBufferHandle source, bgfx::TextureHandle dest, void *destData, uint16_t width, uint16_t height)
{
	const uint8_t viewId = main::PushView(main::s_main->defaultFb, BGFX_CLEAR_NONE, mat4::empty, mat4::empty, Rect());
	bgfx::blit(viewId, dest, 0, 0, bgfx::getTexture(source), 0, 0, width, height);
	bgfx::touch(viewId);
	return bgfx::readTexture(dest, destData);
}

#ifdef DEBUG_HEMICUBE_RENDERING
static uint32_t IntegrateHemicubeBatch(void *batchData, void *weightedBatchData, void *integrationData)
#else
static uint32_t IntegrateHemicubeBatch(void *integrationData)
#endif
{
#ifdef DEBUG_HEMICUBE_RENDERING
	// Read batch texture for debugging.
	AsyncReadTexture(s_lightBakerPersistent->hemicubeFb[0].handle, s_lightBakerPersistent->hemicubeBatchReadTexture, batchData, s_lightBaker->hemicubeBatchSize.x, s_lightBaker->hemicubeBatchSize.y);
#endif

	int fbRead = 0;
	int fbWrite = 1;

	// Weighted downsampling pass.
	bgfx::setTexture(0, s_lightBakerPersistent->hemicubeAtlasSampler.handle, bgfx::getTexture(s_lightBakerPersistent->hemicubeFb[fbRead].handle));
	bgfx::setTexture(1, s_lightBakerPersistent->hemicubeWeightsSampler.handle, s_lightBaker->hemicubeWeightsTexture->getHandle());
	s_lightBakerPersistent->hemicubeWeightsTextureSizeUniform.set(vec4((float)s_lightBaker->hemicubeSize.x, (float)s_lightBaker->hemicubeSize.y, 0, 0));
	main::RenderScreenSpaceQuad("HemicubeWeightedDownsample", s_lightBakerPersistent->hemicubeFb[fbWrite], main::ShaderProgramId::HemicubeWeightedDownsample, BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE, BGFX_CLEAR_COLOR, main::s_main->isTextureOriginBottomLeft, Rect(0, 0, s_lightBaker->hemicubeDownsampleSize.x, s_lightBaker->hemicubeDownsampleSize.y));

#ifdef DEBUG_HEMICUBE_RENDERING
	// Read weighted downsample for debugging.
	AsyncReadTexture(s_lightBakerPersistent->hemicubeFb[fbWrite].handle, s_lightBakerPersistent->hemicubeWeightedBatchReadTexture, weightedBatchData, s_lightBaker->hemicubeDownsampleSize.x, s_lightBaker->hemicubeDownsampleSize.y);
#endif

	// Downsampling passes.
	int outHemiSize = s_lightBaker->hemicubeFaceSize / 2;

	while (outHemiSize > 1)
	{
		const int oldFbRead = fbRead;
		fbRead = fbWrite;
		fbWrite = oldFbRead;

		outHemiSize /= 2;
		bgfx::setTexture(0, s_lightBakerPersistent->hemicubeAtlasSampler.handle, bgfx::getTexture(s_lightBakerPersistent->hemicubeFb[fbRead].handle));
		main::RenderScreenSpaceQuad("HemicubeDownsample", s_lightBakerPersistent->hemicubeFb[fbWrite], main::ShaderProgramId::HemicubeDownsample, BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE, BGFX_CLEAR_COLOR, main::s_main->isTextureOriginBottomLeft, Rect(0, 0, outHemiSize * s_lightBaker->nHemicubesInBatch.x, outHemiSize * s_lightBaker->nHemicubesInBatch.y));
	}

	// Start async texture read of integrated data.
	return AsyncReadTexture(s_lightBakerPersistent->hemicubeFb[fbWrite].handle, s_lightBakerPersistent->hemicubeIntegrationReadTexture, integrationData, s_lightBaker->nHemicubesInBatch.x, s_lightBaker->nHemicubesInBatch.y);
}

bool BakeIndirectLight(uint32_t frameNo)
{
	// Update frame timing and add to history.
	const int frameMs = int((bx::getHPCounter() - s_lightBaker->lastFrameTime) * (1000.0f / (float)bx::getHPFrequency()));
	s_lightBaker->lastFrameTime = bx::getHPCounter();
	s_lightBaker->frameMsHistory[s_lightBaker->frameMsHistoryIndex++] = frameMs;

	if (s_lightBaker->frameMsHistoryIndex >= (int)s_lightBaker->frameMsHistory.size())
	{
		s_lightBaker->frameMsHistoryIndex = 0;
		s_lightBaker->frameMsHistoryFilled = true;
	}

	// Calculate running average frame ms.
	float averageFrameMs = 0;
	const int nFrameMsSamples = s_lightBaker->frameMsHistoryFilled ? (int)s_lightBaker->frameMsHistory.size() : s_lightBaker->frameMsHistoryIndex;

	for (int i = 0; i < nFrameMsSamples; i++)
	{
		averageFrameMs += s_lightBaker->frameMsHistory[i];
	}

	averageFrameMs /= (float)nFrameMsSamples;

	// Adjust the number of hemicubes to render per frame based on the average frame ms.
	if (averageFrameMs < s_lightBaker->maxFrameMs)
	{
		s_lightBaker->nHemicubesToRenderPerFrame++;
	}
	else
	{
		s_lightBaker->nHemicubesToRenderPerFrame--;
	}

	s_lightBaker->nHemicubesToRenderPerFrame = math::Clamped(s_lightBaker->nHemicubesToRenderPerFrame, 1, 32);

	if (!s_lightBaker->finishedHemicubeBatch)
	{
		for (int i = 0; i < s_lightBaker->nHemicubesToRenderPerFrame; i++)
		{
			Luxel luxel = RasterizeLuxel();

			if (luxel.sentinel)
			{
				// Processed all lightmaps.
				s_lightBaker->finishedHemicubeBatch = true;
				s_lightBaker->finishedBakingIndirect = true;
			}
			else
			{
				Lightmap &lightmap = s_lightBaker->lightmaps[luxel.lightmapIndex];

				const vec2i rectOffset
				(
					(s_lightBaker->nHemicubesRenderedInBatch % s_lightBaker->nHemicubesInBatch.x) * s_lightBaker->hemicubeSize.x,
					s_lightBaker->nHemicubesRenderedInBatch / s_lightBaker->nHemicubesInBatch.x * s_lightBaker->hemicubeSize.y
				);

#if 1
				vec3 up(0, 0, 1);
				const float dot = vec3::dotProduct(luxel.normal, up);

				if (dot > 0.8f)
					up = vec3(1, 0, 0);
				else if (dot < -0.8f)
					up = vec3(-1, 0, 0);
#else
				vec3 up(luxel.up);
#endif

				for (int j = 0; j < world::GetNumLightmaps(); j++)
				{
					g_textureCache->alias(world::GetLightmap(j), s_lightBaker->hemicubeLightmaps[j]);
				}

				// Only render lit surfaces after the first bounce.
				main::RenderHemicube(s_lightBakerPersistent->hemicubeFb[0], luxel.position + luxel.normal * 1.0f, luxel.normal, up, rectOffset, s_lightBaker->hemicubeFaceSize, s_lightBaker->currentIndirectBounce > 0);

				for (int j = 0; j < world::GetNumLightmaps(); j++)
				{
					g_textureCache->alias(world::GetLightmap(j), nullptr);
				}

				s_lightBaker->hemicubeBatchLocations[s_lightBaker->nHemicubesRenderedInBatch].lightmap = &lightmap;
				s_lightBaker->hemicubeBatchLocations[s_lightBaker->nHemicubesRenderedInBatch].luxelOffset = luxel.offset;
				s_lightBaker->nHemicubesRenderedInBatch++;

				if (s_lightBaker->nHemicubesRenderedInBatch >= s_lightBaker->nHemicubesInBatch.x * s_lightBaker->nHemicubesInBatch.y)
				{
					s_lightBaker->finishedHemicubeBatch = true;
				}
			}

			if (s_lightBaker->finishedHemicubeBatch)
			{
				// Finished with this batch, integrate and start async texture read.
#ifdef DEBUG_HEMICUBE_RENDERING
				s_lightBaker->hemicubeDataAvailableFrame = IntegrateHemicubeBatch(s_lightBaker->hemicubeBatchData.data(), s_lightBaker->hemicubeWeightedBatchData.data(), s_lightBaker->hemicubeIntegrationData.data());
#else
				s_lightBaker->hemicubeDataAvailableFrame = IntegrateHemicubeBatch(s_lightBaker->hemicubeIntegrationData.data());
#endif
#if 0
				s_lightBaker->finishedBakingIndirect = true;
#endif
				break; // Don't render any more hemicubes this frame when the batch is finished.
			}
		}
	}
	else if (frameNo >= s_lightBaker->hemicubeDataAvailableFrame)
	{
		// Async texture read is ready.
		for (int y = 0; y < s_lightBaker->nHemicubesInBatch.y; y++)
		{
			for (int x = 0; x < s_lightBaker->nHemicubesInBatch.x; x++)
			{
				const int offset = x + y * s_lightBaker->nHemicubesInBatch.x;
				auto rgb = (const vec3 *)&s_lightBaker->hemicubeIntegrationData[offset * 4];
				HemicubeLocation &hemicube = s_lightBaker->hemicubeBatchLocations[offset];
				hemicube.lightmap->passColor[hemicube.luxelOffset] = *rgb * 255.0f * s_lightBaker->indirectLightScale;
			}
		}

#ifdef DEBUG_HEMICUBE_RENDERING
		const std::vector<float> *sources[] = { &s_lightBaker->hemicubeBatchData, &s_lightBaker->hemicubeWeightedBatchData, &s_lightBaker->hemicubeIntegrationData };
		const char *postfixes[] = { "batch", "weighted_batch", "integration" };
		const vec2i sizes[] = { s_lightBaker->hemicubeBatchSize, s_lightBaker->hemicubeDownsampleSize, s_lightBaker->nHemicubesInBatch };

		for (int i = 0; i < 3; i++)
		{
			std::vector<uint8_t> data;
			data.resize(sources[i]->size());

			for (size_t j = 0; j < data.size(); j++)
			{
				data[j] = uint8_t(std::min(sources[i]->data()[j] * 255.0f, 255.0f));
			}

			char filename[MAX_QPATH];
			util::Sprintf(filename, sizeof(filename), "hemicubes/%08d_%s.tga", s_lightBaker->nHemicubeBatchesProcessed, postfixes[i]);
			stbi_write_tga(filename, sizes[i].x, sizes[i].y, 4, data.data());
		}
#endif

		s_lightBaker->nHemicubeBatchesProcessed++;

		if (s_lightBaker->finishedBakingIndirect)
			return false;

		// Reset state to start the next batch.
		s_lightBaker->finishedHemicubeBatch = false;
		s_lightBaker->nHemicubesRenderedInBatch = 0;
	}

	// Update progress.
	const int progress = int(GetNumRasterizedTriangles() / (float)s_lightBaker->totalLightmappedTriangles * 100.0f);

	if (progress != s_lightBaker->indirectLightProgress)
	{
		s_lightBaker->indirectLightProgress = progress;
		SetStatus(LightBaker::Status::BakingIndirectLight_Running, progress);
	}

	return true;
}

} // namespace light_baker
} // namespace renderer
#endif // USE_LIGHT_BAKER
