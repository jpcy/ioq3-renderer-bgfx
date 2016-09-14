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

#ifdef __GNUC__
#ifndef __forceinline
#define __forceinline
#endif
#endif
#include "../embree2/rtcore.h"
#include "../embree2/rtcore_ray.h"

//#define DEBUG_HEMICUBE_RENDERING

namespace renderer {
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
	vec3 position;
	vec3 normal;
	vec3 up;
	int offset; // Offset into Lightmap::color.
};

struct Lightmap
{
	std::vector<Luxel> luxels; // sparse
	std::vector<vec4> color; // width * height
	std::vector<uint8_t> colorBytes; // width * height * 4
};

struct HemicubeLocation
{
	Lightmap *lightmap;
	Luxel *luxel;
};

struct LightBaker
{
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
	int nLuxelsProcessed; // used by direct and indirect passes to measure progress
	int totalLuxels; // calculated when rasterizing
	std::vector<Lightmap> lightmaps;
	static const size_t maxErrorMessageLen = 2048;
	char errorMessage[maxErrorMessageLen];

	// embree
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
	int currentLightmapIndex, currentLuxelIndex;
	uint32_t hemicubeDataAvailableFrame;
#ifdef DEBUG_HEMICUBE_RENDERING
	std::vector<float> hemicubeBatchData;
	std::vector<float> hemicubeWeightedBatchData;
#endif
	std::vector<float> hemicubeIntegrationData;
	std::vector<HemicubeLocation> hemicubeBatchLocations;
	int indirectLightProgress;
	const float indirectLightScale = 0.25f;
	const int nIndirectPasses = 1;
	int currentIndirectPass = 0;

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

	~LightBaker()
	{
		if (mutex)
			SDL_DestroyMutex(mutex);
	}
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

void RasterizeLightmappedSurfaces();

} // namespace light_baker
} // namespace renderer
