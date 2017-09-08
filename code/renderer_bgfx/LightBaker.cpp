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
#include "World.h"

#include "stb_image_write.h"

namespace renderer {
namespace light_baker {

std::unique_ptr<LightBaker> s_lightBaker;
std::unique_ptr<LightBakerPersistent> s_lightBakerPersistent;
std::vector<AreaLight> s_areaLights;

static void lmImageDilate(const float *image, float *outImage, int w, int h, int c)
{
	assert(c > 0 && c <= 4);
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			float color[4];
			bool valid = false;
			for (int i = 0; i < c; i++)
			{
				color[i] = image[(y * w + x) * c + i];
				valid |= color[i] > 0.0f;
			}
			if (!valid)
			{
				int n = 0;
				const int dx[] = { -1, 0, 1,  0 };
				const int dy[] = { 0, 1, 0, -1 };
				for (int d = 0; d < 4; d++)
				{
					int cx = x + dx[d];
					int cy = y + dy[d];
					if (cx >= 0 && cx < w && cy >= 0 && cy < h)
					{
						float dcolor[4];
						bool dvalid = false;
						for (int i = 0; i < c; i++)
						{
							dcolor[i] = image[(cy * w + cx) * c + i];
							dvalid |= dcolor[i] > 0.0f;
						}
						if (dvalid)
						{
							for (int i = 0; i < c; i++)
								color[i] += dcolor[i];
							n++;
						}
					}
				}
				if (n)
				{
					float in = 1.0f / n;
					for (int i = 0; i < c; i++)
						color[i] *= in;
				}
			}
			for (int i = 0; i < c; i++)
				outImage[(y * w + x) * c + i] = color[i];
		}
	}
}

bool IsSurfaceLightmapped(const world::Surface &surface)
{
	return surface.type != world::SurfaceType::Ignore && surface.type != world::SurfaceType::Flare && surface.material->lightmapIndex >= 0;
}

static void ClearPassColor()
{
	for (Lightmap &lightmap : s_lightBaker->lightmaps)
	{
		memset(lightmap.passColor.data(), 0, lightmap.passColor.size() * sizeof(vec3));
	}
}

static void AccumulatePassColor()
{
	const vec2i lightmapSize = world::GetLightmapSize();

#define DILATE
#ifdef DILATE
	std::vector<vec3> dilatedLightmap;
	dilatedLightmap.resize(lightmapSize.x * lightmapSize.y);
#endif

	for (Lightmap &lightmap : s_lightBaker->lightmaps)
	{
#ifdef DILATE
		lmImageDilate(&lightmap.passColor[0].x, &dilatedLightmap[0].x, lightmapSize.x, lightmapSize.y, 3);
#endif

		for (int i = 0; i < lightmapSize.x * lightmapSize.y; i++)
		{
#ifdef DILATE
			const vec3 &src = dilatedLightmap[i];
#else
			const vec3 &src = lightmap.passColor[i];
#endif

			lightmap.accumulatedColor[i] += src;
		}
	}
}

static void EncodeLightmaps()
{
	const vec2i lightmapSize = world::GetLightmapSize();

	for (Lightmap &lightmap : s_lightBaker->lightmaps)
	{
		for (int i = 0; i < lightmapSize.x * lightmapSize.y; i++)
		{
			// Colors are floats, but in 0-255+ range.
			lightmap.encodedPassColor[i] = vec4b(vec4(util::OverbrightenColor(lightmap.passColor[i] / 255.0f), 1));
			lightmap.encodedAccumulatedColor[i] = vec4b(vec4(util::OverbrightenColor(lightmap.accumulatedColor[i] / 255.0f), 1));
		}
	}
}

/*
================================================================================
LIGHT BAKER THREAD AND INTERFACE
================================================================================
*/

static bool ThreadUpdateLightmaps()
{
	// Update lightmaps. Wait for it to finish before continuing.
	EncodeLightmaps();
	SetStatus(LightBaker::Status::TextureUpdateWaiting);

	for (;;)
	{
		// Check for cancelling.
		if (GetStatus() == LightBaker::Status::Cancelled)
			return false;
		else if (GetStatus() == LightBaker::Status::TextureUpdateFinished)
			break;

		SDL_Delay(50);
	}

	return true;
}

static int Thread(void *data)
{
	SetStatus(LightBaker::Status::BakingDirectLight);

	// Allocate lightmap memory.
	const vec2i lightmapSize = world::GetLightmapSize();
	s_lightBaker->lightmaps.resize(world::GetNumLightmaps());

	for (Lightmap &lightmap : s_lightBaker->lightmaps)
	{
		lightmap.duplicateBits.resize(lightmapSize.x * lightmapSize.y / 8);
		lightmap.passColor.resize(lightmapSize.x * lightmapSize.y);
		lightmap.accumulatedColor.resize(lightmapSize.x * lightmapSize.y);
		lightmap.encodedPassColor.resize(lightmapSize.x * lightmapSize.y);
		lightmap.encodedAccumulatedColor.resize(lightmapSize.x * lightmapSize.y);

#ifdef DEBUG_LIGHTMAP_INTERPOLATION
		lightmap.interpolationDebug.resize(lightmapSize.x * lightmapSize.y * 3);
#endif
	}

	// Count the total number of lightmapped triangles for measuring progress.
	s_lightBaker->totalLightmappedTriangles = 0;

	for (int mi = 0; mi < world::GetNumModels(); mi++)
	{
		for (int si = 0; si < world::GetNumSurfaces(mi); si++)
		{
			const world::Surface &surface = world::GetSurface(mi, si);

			if (IsSurfaceLightmapped(surface))
			{
				s_lightBaker->totalLightmappedTriangles += int(surface.indices.size() / 3);
			}
		}
	}

	// Bake direct lighting.
	ClearPassColor();

	if (!InitializeDirectLight())
		return 1;

	if (!BakeDirectLight())
		return 1;

	AccumulatePassColor();

	// Update the lightmaps after the direct lighting pass has finished. Indirect lighting needs to render using the updated lightmaps.
	if (!ThreadUpdateLightmaps())
		return 1;

	// Ready for indirect lighting, which happens in the main thread. Wait until it finishes.
	for (int i = 0; i < s_lightBaker->nIndirectBounces; i++)
	{
		ClearPassColor();

#ifdef DEBUG_LIGHTMAP_INTERPOLATION
		for (Lightmap &lightmap : s_lightBaker->lightmaps)
		{
			memset(lightmap.interpolationDebug.data(), 0, lightmap.interpolationDebug.size());
		}
#endif

		SetStatus(LightBaker::Status::BakingIndirectLight_Started);

		for (;;)
		{
			// Check for cancelling.
			if (GetStatus() == LightBaker::Status::Cancelled)
				return 1;
			else if (GetStatus() == LightBaker::Status::BakingIndirectLight_Finished)
				break;

			SDL_Delay(50);
		}

		AccumulatePassColor();

#ifdef DEBUG_LIGHTMAP_INTERPOLATION
		for (int j = 0; j < (int)s_lightBaker->lightmaps.size(); j++)
		{
			const Lightmap &lightmap = s_lightBaker->lightmaps[j];
			char filename[MAX_QPATH];
			util::Sprintf(filename, sizeof(filename), "interpolation_lightmap%02d_bounce%02d.tga", j, i + 1);
			stbi_write_tga(filename, lightmapSize.x, lightmapSize.y, 3, lightmap.interpolationDebug.data());
		}
#endif

		if (!ThreadUpdateLightmaps())
			return 1;
	}

	SetStatus(LightBaker::Status::Finished);
	return 0;
}

void Start(int nSamples)
{
	if (s_lightBaker.get() || !world::IsLoaded())
		return;

	s_lightBaker = std::make_unique<LightBaker>();
	s_lightBakerPersistent = std::make_unique<LightBakerPersistent>();

	if (!InitializeEmbree())
	{
		s_lightBaker.reset();
		return;
	}

	LoadAreaLightTextures();
	s_lightBaker->nSamples = math::Clamped(nSamples, 1, (int)LightBaker::maxSamples);
	s_lightBaker->startTime = bx::getHPCounter();
	s_lightBaker->mutex = SDL_CreateMutex();

	if (!s_lightBaker->mutex)
	{
		interface::PrintWarningf("Creating light baker mutex failed. Reason: \"%s\"", SDL_GetError());
		ShutdownEmbree();
		s_lightBaker.reset();
		return;
	}

	s_lightBaker->thread = SDL_CreateThread(Thread, "LightBaker", nullptr);

	if (!s_lightBaker->thread)
	{
		interface::PrintWarningf("Creating light baker thread failed. Reason: \"%s\"", SDL_GetError());
		ShutdownEmbree();
		s_lightBaker.reset();
		return;
	}
}

void Stop()
{
	if (!s_lightBaker.get())
		return;

	if (s_lightBaker->thread)
	{
		const LightBaker::Status status = GetStatus();

		if (status != LightBaker::Status::Finished && status != LightBaker::Status::Error)
			SetStatus(LightBaker::Status::Cancelled);

		SDL_WaitThread(s_lightBaker->thread, NULL);
	}

	ShutdownEmbree();
	s_lightBaker.reset();
}

void Shutdown()
{
	Stop();

	if (!s_lightBakerPersistent.get())
		return;

	if (bgfx::isValid(s_lightBakerPersistent->hemicubeIntegrationReadTexture))
	{
		bgfx::destroy(s_lightBakerPersistent->hemicubeIntegrationReadTexture);
	}

	s_lightBakerPersistent.reset();
}

LightBaker::Status GetStatus(int *progress)
{
	auto status = LightBaker::Status::NotStarted;

	if (SDL_LockMutex(s_lightBaker->mutex) == 0)
	{
		status = s_lightBaker->status;

		if (progress)
			*progress = s_lightBaker->progress;

		SDL_UnlockMutex(s_lightBaker->mutex);
	}
	else
	{
		if (progress)
			*progress = 0;
	}

	return status;
}

void SetStatus(LightBaker::Status status, int progress)
{
	if (SDL_LockMutex(s_lightBaker->mutex) == 0)
	{
		s_lightBaker->status = status;
		s_lightBaker->progress = progress;
		SDL_UnlockMutex(s_lightBaker->mutex);
	}
}

static float ToSeconds(int64_t t)
{
	return t * (1.0f / (float)bx::getHPFrequency());
}

void Update(uint32_t frameNo)
{
	if (!s_lightBaker.get())
	{
#if 0
		for (size_t i = 0; i < s_areaLights.size(); i++)
		{
			for (size_t j = 0; j < s_areaLights[i].samples.size(); j++)
				main::DrawAxis(s_areaLights[i].samples[j].position);
		}
#endif

		return;
	}

	int progress;
	LightBaker::Status status = GetStatus(&progress);

	if (status == LightBaker::Status::BakingDirectLight)
	{
		main::DebugPrint("Baking direct lighting... %d", progress);
	}
	else if (status == LightBaker::Status::BakingIndirectLight_Started)
	{
		InitializeIndirectLight();

		if (s_lightBaker->currentIndirectBounce == 0)
		{
			s_lightBaker->indirectBakeStartTime = bx::getHPCounter();
		}

		SetStatus(LightBaker::Status::BakingIndirectLight_Running);
		s_lightBaker->lastFrameTime = bx::getHPCounter();
	}
	else if (status == LightBaker::Status::BakingIndirectLight_Running)
	{
		main::DebugPrint("Baking indirect lighting... %d", progress);

		if (!BakeIndirectLight(frameNo))
		{
			if (s_lightBaker->currentIndirectBounce == s_lightBaker->nIndirectBounces - 1)
			{
				s_lightBaker->indirectBakeTime = bx::getHPCounter() - s_lightBaker->indirectBakeStartTime;
			}

			s_lightBaker->currentIndirectBounce++;
			SetStatus(LightBaker::Status::BakingIndirectLight_Finished);
		}
	}
	else if (status == LightBaker::Status::TextureUpdateWaiting)
	{
		if (s_lightBaker->textureUpdateFrameNo == 0)
		{
			// Update lightmap textures.
			const vec2i lightmapSize = world::GetLightmapSize();

			for (int i = 0; i < world::GetNumLightmaps(); i++)
			{
				const Lightmap &lightmap = s_lightBaker->lightmaps[i];
				Texture *texture = world::GetLightmap(i);
				texture->update(bgfx::makeRef(lightmap.encodedAccumulatedColor.data(), uint32_t(lightmap.encodedAccumulatedColor.size() * sizeof(vec4b))), 0, 0, lightmapSize.x, lightmapSize.y);

				if (!s_lightBaker->hemicubeLightmaps.empty())
				{
					texture = s_lightBaker->hemicubeLightmaps[i];
					texture->update(bgfx::makeRef(lightmap.encodedPassColor.data(), uint32_t(lightmap.encodedPassColor.size() * sizeof(vec4b))), 0, 0, lightmapSize.x, lightmapSize.y);
				}
			}

			s_lightBaker->textureUpdateFrameNo = frameNo;
		}
		else if (frameNo > s_lightBaker->textureUpdateFrameNo + BGFX_NUM_BUFFER_FRAMES)
		{
			// bgfx has finished updating the texture data by now.
			SetStatus(LightBaker::Status::TextureUpdateFinished);
			s_lightBaker->textureUpdateFrameNo = 0;
		}
	}
	else if (status == LightBaker::Status::Finished)
	{
		interface::Printf("Finished baking lights\n");
		interface::Printf("   %0.2f seconds elapsed\n", ToSeconds(bx::getHPCounter() - s_lightBaker->startTime));
		interface::Printf("      %0.2f seconds for rasterization\n", ToSeconds(s_lightBaker->rasterizationTime));
		interface::Printf("      %0.2f seconds for direct lighting\n", ToSeconds(s_lightBaker->directBakeTime));
		interface::Printf("      %0.2f seconds for indirect lighting\n", ToSeconds(s_lightBaker->indirectBakeTime));
		interface::Printf("   %d luxels\n", s_lightBaker->totalLuxels);
		interface::Printf("   %0.2f ms elapsed per luxel\n", (bx::getHPCounter() - s_lightBaker->startTime) * (1000.0f / (float)bx::getHPFrequency()) / s_lightBaker->totalLuxels);
		interface::Printf("   %d hemicube batches\n", s_lightBaker->nHemicubeBatchesProcessed);
		interface::Printf("   %d interpolated luxels\n", s_lightBaker->nInterpolatedLuxels);
		interface::Printf("   %d area lights\n", (int)s_areaLights.size());
		interface::Printf("   %d entity lights\n", (int)s_lightBaker->lights.size());
		Stop();
	}
	else if (status == LightBaker::Status::Error)
	{
		interface::PrintWarningf("%s\n", s_lightBaker->errorMessage);
		Stop();
	}
}

} // namespace light_baker
} // namespace renderer
#endif // USE_LIGHT_BAKER
