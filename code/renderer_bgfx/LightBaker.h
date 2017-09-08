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
#pragma once

#if defined(USE_LIGHT_BAKER)
#include "Precompiled.h"
#pragma hdrstop

#ifdef __GNUC__
#ifndef __forceinline
#define __forceinline inline
#endif
#endif
#include "../embree2/rtcore.h"
#include "../embree2/rtcore_ray.h"

#undef Status // unknown source. affects linux build.

//#define DEBUG_HEMICUBE_RENDERING
//#define DEBUG_LIGHTMAP_INTERPOLATION

namespace renderer {

namespace world {
struct Surface;
}

namespace light_baker {

struct AreaLightTexture
{
	const Material *material;
	int width, height, nComponents;
	std::vector<uint8_t> data;
	vec4 normalizedColor;
	vec4 averageColor;
};

struct Winding
{
	static const int maxPoints = 8;
	vec3 p[maxPoints];
	int numpoints;
};

struct AreaLightSample
{
	vec3 position;
	vec2 texCoord;
	float photons;
	Winding winding;
};

struct AreaLight
{
	int modelIndex;
	int surfaceIndex;
	//Material *material;
	const AreaLightTexture *texture;
	std::vector<AreaLightSample> samples;
};

struct FaceFlags
{
	enum
	{
		Sky = 1<<0
	};
};

struct Luxel
{
	bool sentinel; // True if finished rasterization.
	int lightmapIndex;
	vec3 position;
	vec3 normal;
	vec3 up;
	int offset; // Offset into Lightmap::color.
};

struct Lightmap
{
	/// @brief Avoid rasterizing the same luxel with different primitives.
	std::vector<uint8_t> duplicateBits;

	/// @brief Color data for this pass (direct or an indirect bounce).
	std::vector<vec3> passColor;

	/// @brief Accumulated color data of all passes.
	std::vector<vec3> accumulatedColor;

	/// @brief Pass color data encoded to RGBM.
	std::vector<vec4b> encodedPassColor;

	/// @brief Accumulated color data encoded to RGBM.
	std::vector<vec4b> encodedAccumulatedColor;

#ifdef DEBUG_LIGHTMAP_INTERPOLATION
	std::vector<uint8_t> interpolationDebug; // width * height * 3
#endif
};

struct HemicubeLocation
{
	Lightmap *lightmap;
	int luxelOffset;
};

struct LightBaker
{
	~LightBaker()
	{
		if (mutex)
			SDL_DestroyMutex(mutex);
	}

	enum class Status
	{
		NotStarted,
		BakingDirectLight,
		BakingIndirectLight_Started,
		BakingIndirectLight_Running,
		BakingIndirectLight_Finished,
		TextureUpdateWaiting,
		TextureUpdateFinished,
		Finished,
		Cancelled,
		Error
	};

	SDL_mutex *mutex = nullptr;
	SDL_Thread *thread;

	// area lights
	std::vector<AreaLightTexture> areaLightTextures;
	//std::vector<LightBaker::AreaLight> areaLights;
	std::vector<uint8_t> areaLightVisData;
	int areaLightClusterBytes;

	// entity lights (point and spot)
	vec3 ambientLight;
	std::vector<StaticLight> lights;

	// jitter/multisampling
	int nSamples;
	static const int maxSamples = 16;
	std::array<vec3, maxSamples> dirJitter, posJitter;

	// profiling
	int64_t startTime, rasterizationTime, directBakeTime, indirectBakeStartTime, indirectBakeTime;

	uint32_t textureUpdateFrameNo = 0; // lightmap textures were updated this frame
	int totalLuxels; // calculated when rasterizing direct light (since it doesn't use interpolation)
	int totalLightmappedTriangles; // used to measure progress
	std::vector<Lightmap> lightmaps;
	static const size_t maxErrorMessageLen = 2048;
	char errorMessage[maxErrorMessageLen];

	// embree
	void *embreeLibrary = nullptr;
	RTCDevice embreeDevice = nullptr;
	RTCScene embreeScene = nullptr;
	std::vector<uint8_t> faceFlags;

	// hemicube / indirect light
	const int hemicubeFaceSize = 64;
	const vec2i hemicubeSize = vec2i(hemicubeFaceSize * 3, hemicubeFaceSize);
	const vec2i nHemicubesInBatch = vec2i(8, 16);
	const vec2i hemicubeBatchSize = hemicubeSize * nHemicubesInBatch;
	const vec2i hemicubeDownsampleSize = vec2i(nHemicubesInBatch.x * hemicubeFaceSize / 2, nHemicubesInBatch.y * hemicubeFaceSize / 2);
	const Texture *hemicubeWeightsTexture;
	int nHemicubesToRenderPerFrame;
	int nHemicubesRenderedInBatch;
	int nHemicubeBatchesProcessed;
	bool finishedHemicubeBatch;
	bool finishedBakingIndirect;
	uint32_t hemicubeDataAvailableFrame;
#ifdef DEBUG_HEMICUBE_RENDERING
	std::vector<float> hemicubeBatchData;
	std::vector<float> hemicubeWeightedBatchData;
#endif
	std::vector<float> hemicubeIntegrationData;
	std::vector<HemicubeLocation> hemicubeBatchLocations;
	int indirectLightProgress;
	const float indirectLightScale = 0.25f;
	const int nIndirectBounces = 1;
	int currentIndirectBounce = 0;
	int nInterpolatedLuxels = 0;

	/// @brief Lightmaps to use when rendering hemicubes.
	/// @remarks The result from the previous pass (direct or indirect bounce) only, not accumulated with all previous passes.
	std::vector<Texture *> hemicubeLightmaps;

	// Indirect light frame ms measuring. Determines how many hemicubes to render per frame. Don't want to freeze, maintain a low frame rate instead.
	int64_t lastFrameTime;
	std::array<int, 50> frameMsHistory; // frame ms for the last n frames
	int frameMsHistoryIndex = 0; // incremented every frame, wrapping back to 0
	bool frameMsHistoryFilled = false; // true when frameMsHistoryIndex has wrapped
	const int maxFrameMs = 40; // 25Hz

	// mutex protected state
	Status status;
	int progress = 0;

	const float areaScale = 0.25f;
	const float formFactorValueScale = 3.0f;
	const float pointScale = 7500.0f;
	const float linearScale = 1.0f / 8000.0f;
};

struct LightBakerPersistent
{
	FrameBuffer hemicubeFb[2];

#ifdef DEBUG_HEMICUBE_RENDERING
	bgfx::TextureHandle hemicubeBatchReadTexture = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle hemicubeWeightedBatchReadTexture = BGFX_INVALID_HANDLE;
#endif

	bgfx::TextureHandle hemicubeIntegrationReadTexture = BGFX_INVALID_HANDLE;

	/// @remarks Only x and y used.
	Uniform_vec4 hemicubeWeightsTextureSizeUniform = "u_HemicubeWeightsTextureSize";
	Uniform_int hemicubeAtlasSampler = "u_HemicubeAtlas";
	Uniform_int hemicubeWeightsSampler = "u_HemicubeWeights";
};

extern std::unique_ptr<LightBaker> s_lightBaker;
extern std::unique_ptr<LightBakerPersistent> s_lightBakerPersistent;
extern std::vector<AreaLight> s_areaLights;

bool InitializeEmbree();
void ShutdownEmbree();
void LoadAreaLightTextures();
bool InitializeDirectLight();
bool BakeDirectLight();

void InitializeIndirectLight();
bool BakeIndirectLight(uint32_t frameNo);

void InitializeRasterization(int interpolationPasses, float interpolationThreshold);
Luxel RasterizeLuxel();
int GetNumRasterizedTriangles();

bool IsSurfaceLightmapped(const world::Surface &surface);
LightBaker::Status GetStatus(int *progress = nullptr);
void SetStatus(LightBaker::Status status, int progress = 0);

} // namespace light_baker
} // namespace renderer
#endif // USE_LIGHT_BAKER
