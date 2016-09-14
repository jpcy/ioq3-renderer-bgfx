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
/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */
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
#include "LightBaker.h"
#include "Main.h" // need to access main internals for hemicube integration
#include "World.h"

#include "stb_image_resize.h"
#include "stb_image_write.h"
#undef Status // unknown source. affects linux build.

namespace renderer {
namespace light_baker {

std::unique_ptr<LightBaker> s_lightBaker;
std::unique_ptr<LightBakerPersistent> s_lightBakerPersistent;
static std::vector<AreaLight> s_areaLights;

// optional: set material characteristics by specifying cos(theta)-dependent weights for incoming light.
typedef float (*lm_weight_func)(float cos_theta, void *userdata);

static float lm_defaultWeights(float cos_theta, void *userdata)
{
	return 1.0f;
}

static bool DoesSurfaceOccludeLight(const world::Surface &surface)
{
	// Translucent surfaces (e.g. flames) shouldn't occlude.
	// Not handling alpha-testing yet.
	return surface.type != world::SurfaceType::Ignore && surface.type != world::SurfaceType::Flare && (surface.contentFlags & CONTENTS_TRANSLUCENT) == 0 && (surface.flags & SURF_ALPHASHADOW) == 0;
}

/*
================================================================================
LIGHT BAKER THREAD STATUS
================================================================================
*/

static LightBaker::Status GetStatus(int *progress = nullptr)
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

static void SetStatus(LightBaker::Status status, int progress = 0)
{
	if (SDL_LockMutex(s_lightBaker->mutex) == 0)
	{
		s_lightBaker->status = status;
		s_lightBaker->progress = progress;
		SDL_UnlockMutex(s_lightBaker->mutex);
	}
}

/*
================================================================================
EMBREE
================================================================================
*/

#if defined(WIN32)
#define EMBREE_LIB "embree.dll"
#else
#define EMBREE_LIB "libembree.so"
#endif

extern "C"
{
	typedef RTCDevice (*EmbreeNewDevice)(const char* cfg);
	typedef void (*EmbreeDeleteDevice)(RTCDevice device);
	typedef RTCError (*EmbreeDeviceGetError)(RTCDevice device);
	typedef RTCScene (*EmbreeDeviceNewScene)(RTCDevice device, RTCSceneFlags flags, RTCAlgorithmFlags aflags);
	typedef void (*EmbreeDeleteScene)(RTCScene scene);
	typedef unsigned (*EmbreeNewTriangleMesh)(RTCScene scene, RTCGeometryFlags flags, size_t numTriangles, size_t numVertices, size_t numTimeSteps);
	typedef void* (*EmbreeMapBuffer)(RTCScene scene, unsigned geomID, RTCBufferType type);
	typedef void (*EmbreeUnmapBuffer)(RTCScene scene, unsigned geomID, RTCBufferType type);
	typedef void (*EmbreeCommit)(RTCScene scene);
	typedef void (*EmbreeOccluded)(RTCScene scene, RTCRay& ray);
	typedef void (*EmbreeIntersect)(RTCScene scene, RTCRay& ray);

	static EmbreeNewDevice embreeNewDevice = nullptr;
	static EmbreeDeleteDevice embreeDeleteDevice = nullptr;
	static EmbreeDeviceGetError embreeDeviceGetError = nullptr;
	static EmbreeDeviceNewScene embreeDeviceNewScene = nullptr;
	static EmbreeDeleteScene embreeDeleteScene = nullptr;
	static EmbreeNewTriangleMesh embreeNewTriangleMesh = nullptr;
	static EmbreeMapBuffer embreeMapBuffer = nullptr;
	static EmbreeUnmapBuffer embreeUnmapBuffer = nullptr;
	static EmbreeCommit embreeCommit = nullptr;
	static EmbreeOccluded embreeOccluded = nullptr;
	static EmbreeIntersect embreeIntersect = nullptr;
}

#define EMBREE_FUNCTION(func, type, name) func = (type)SDL_LoadFunction(so, name); if (!func) { interface::PrintWarningf("Error loading Embree function %s\n", name); return false; }

static bool LoadEmbreeLibrary()
{
	// Load embree library if not already loaded.
	if (embreeNewDevice)
		return true;

	void *so = SDL_LoadObject(EMBREE_LIB);

	if (!so)
	{
		interface::PrintWarningf("%s\n", SDL_GetError());
		return false;
	}

	EMBREE_FUNCTION(embreeNewDevice, EmbreeNewDevice, "rtcNewDevice");
	EMBREE_FUNCTION(embreeDeleteDevice, EmbreeDeleteDevice, "rtcDeleteDevice");
	EMBREE_FUNCTION(embreeDeviceGetError, EmbreeDeviceGetError, "rtcDeviceGetError");
	EMBREE_FUNCTION(embreeDeviceNewScene, EmbreeDeviceNewScene, "rtcDeviceNewScene");
	EMBREE_FUNCTION(embreeDeleteScene, EmbreeDeleteScene, "rtcDeleteScene");
	EMBREE_FUNCTION(embreeNewTriangleMesh, EmbreeNewTriangleMesh, "rtcNewTriangleMesh");
	EMBREE_FUNCTION(embreeMapBuffer, EmbreeMapBuffer, "rtcMapBuffer");
	EMBREE_FUNCTION(embreeUnmapBuffer, EmbreeUnmapBuffer, "rtcUnmapBuffer");
	EMBREE_FUNCTION(embreeCommit, EmbreeCommit, "rtcCommit");
	EMBREE_FUNCTION(embreeOccluded, EmbreeOccluded, "rtcOccluded");
	EMBREE_FUNCTION(embreeIntersect, EmbreeIntersect, "rtcIntersect");
	return true;
}

static const char *s_embreeErrorStrings[] =
{
	"RTC_NO_ERROR",
	"RTC_UNKNOWN_ERROR",
	"RTC_INVALID_ARGUMENT",
	"RTC_INVALID_OPERATION",
	"RTC_OUT_OF_MEMORY",
	"RTC_UNSUPPORTED_CPU",
	"RTC_CANCELLED"
};

static int CheckEmbreeError(const char *lastFunctionName)
{
	RTCError error = embreeDeviceGetError(s_lightBaker->embreeDevice);

	if (error != RTC_NO_ERROR)
	{
		bx::snprintf(s_lightBaker->errorMessage, LightBaker::maxErrorMessageLen, "Embree error: %s returned %s", lastFunctionName, s_embreeErrorStrings[error]);
		return 1;
	}

	return 0;
}

#define CHECK_EMBREE_ERROR(name) { const int r = CheckEmbreeError(#name); if (r) return false; }

struct Triangle
{
	uint32_t indices[3];
};

static bool CreateEmbreeGeometry(int nVertices, int nTriangles)
{
	// Indices are 32-bit. The World representation uses 16-bit indices, with multiple vertex buffers on large maps that don't fit into one. Combine those here.
	s_lightBaker->embreeDevice = embreeNewDevice(nullptr);
	CHECK_EMBREE_ERROR(embreeNewDevice)
	s_lightBaker->embreeScene = embreeDeviceNewScene(s_lightBaker->embreeDevice, RTC_SCENE_STATIC, RTC_INTERSECT1);
	CHECK_EMBREE_ERROR(embreeDeviceNewScene)
	unsigned int embreeMesh = embreeNewTriangleMesh(s_lightBaker->embreeScene, RTC_GEOMETRY_STATIC, nTriangles, nVertices, 1);
	CHECK_EMBREE_ERROR(embreeNewTriangleMesh);

	auto vertices = (vec4 *)embreeMapBuffer(s_lightBaker->embreeScene, embreeMesh, RTC_VERTEX_BUFFER);
	CHECK_EMBREE_ERROR(embreeMapBuffer);
	size_t currentVertex = 0;

	for (int i = 0; i < world::GetNumVertexBuffers(); i++)
	{
		const std::vector<Vertex> &worldVertices = world::GetVertexBuffer(i);

		for (size_t vi = 0; vi < worldVertices.size(); vi++)
		{
			vertices[currentVertex] = worldVertices[vi].pos;
			currentVertex++;
		}
	}

	embreeUnmapBuffer(s_lightBaker->embreeScene, embreeMesh, RTC_VERTEX_BUFFER);
	CHECK_EMBREE_ERROR(embreeUnmapBuffer);

	auto triangles = (Triangle *)embreeMapBuffer(s_lightBaker->embreeScene, embreeMesh, RTC_INDEX_BUFFER);
	CHECK_EMBREE_ERROR(embreeMapBuffer);
	size_t faceIndex = 0;
	s_lightBaker->faceFlags.resize(nTriangles);

	for (int si = 0; si < world::GetNumSurfaces(0); si++)
	{
		const world::Surface &surface = world::GetSurface(0, si);

		if (!DoesSurfaceOccludeLight(surface))
			continue;

		// Adjust for combining multiple vertex buffers.
		uint32_t indexOffset = 0;

		for (size_t i = 0; i < surface.bufferIndex; i++)
			indexOffset += (uint32_t)world::GetVertexBuffer(i).size();

		for (size_t i = 0; i < surface.indices.size(); i += 3)
		{
			if (surface.material->isSky || (surface.flags & SURF_SKY))
				s_lightBaker->faceFlags[faceIndex] |= FaceFlags::Sky;

			triangles[faceIndex].indices[0] = indexOffset + surface.indices[i + 0];
			triangles[faceIndex].indices[1] = indexOffset + surface.indices[i + 1];
			triangles[faceIndex].indices[2] = indexOffset + surface.indices[i + 2];
			faceIndex++;
		}
	}

	embreeUnmapBuffer(s_lightBaker->embreeScene, embreeMesh, RTC_INDEX_BUFFER);
	CHECK_EMBREE_ERROR(embreeUnmapBuffer);
	embreeCommit(s_lightBaker->embreeScene);
	CHECK_EMBREE_ERROR(embreeCommit);
	return true;
}

static void DestroyEmbreeGeometry()
{
	if (s_lightBaker->embreeScene)
		embreeDeleteScene(s_lightBaker->embreeScene);

	if (s_lightBaker->embreeDevice)
		embreeDeleteDevice(s_lightBaker->embreeDevice);
}

/*
================================================================================
ENTITY LIGHTS
================================================================================
*/

static vec3 ParseColorString(const char *color)
{
	vec3 rgb;
	sscanf(color, "%f %f %f", &rgb.r, &rgb.g, &rgb.b);

	// Normalize. See q3map ColorNormalize.
	const float max = std::max(rgb.r, std::max(rgb.g, rgb.b));
	return rgb * (1.0f / max);
}

static void CalculateLightEnvelope(StaticLight *light)
{
	assert(light);

	// From q3map2 SetupEnvelopes.
	const float falloffTolerance = 1.0f;

	if (!(light->flags & StaticLightFlags::DistanceAttenuation))
	{
		light->envelope = FLT_MAX;
	}
	else if (light->flags & StaticLightFlags::LinearAttenuation)
	{
		light->envelope = light->photons * s_lightBaker->linearScale - falloffTolerance;
	}
	else
	{
		light->envelope = sqrt(light->photons / falloffTolerance);
	}
}

static void CreateEntityLights()
{
	for (size_t i = 0; i < world::s_world->entities.size(); i++)
	{
		const world::Entity &entity = world::s_world->entities[i];
		const char *classname = entity.findValue("classname", "");

		if (!util::Stricmp(classname, "worldspawn"))
		{
			const char *color = entity.findValue("_color");
			const char *ambient = entity.findValue("ambient");
					
			if (color && ambient)
				s_lightBaker->ambientLight = ParseColorString(color) * (float)atof(ambient);

			continue;
		}
		else if (util::Stricmp(classname, "light"))
			continue;

		StaticLight light;
		const char *color = entity.findValue("_color");
					
		if (color)
		{
			light.color = vec4(ParseColorString(color), 1);
		}
		else
		{
			light.color = vec4::white;
		}

		light.intensity = (float)atof(entity.findValue("light", "300"));
		light.photons = light.intensity * s_lightBaker->pointScale;
		const char *origin = entity.findValue("origin");

		if (!origin)
			continue;

		sscanf(origin, "%f %f %f", &light.position.x, &light.position.y, &light.position.z);
		light.radius = (float)atof(entity.findValue("radius", "64"));
		const int spawnFlags = atoi(entity.findValue("spawnflags", "0"));
		light.flags = StaticLightFlags::DefaultMask;

		// From q3map2 CreateEntityLights.
		if (spawnFlags & 1)
		{
			// Linear attenuation.
			light.flags |= StaticLightFlags::LinearAttenuation;
			light.flags &= ~StaticLightFlags::AngleAttenuation;
		}

		if (spawnFlags & 2)
		{
			// No angle attenuation.
			light.flags &= ~StaticLightFlags::AngleAttenuation;
		}

		// Find target (spotlights).
		const char *target = entity.findValue("target");

		if (target)
		{
			// Spotlights always use angle attenuation, never linear.
			light.flags |= StaticLightFlags::Spotlight | StaticLightFlags::AngleAttenuation;
			light.flags &= ~StaticLightFlags::LinearAttenuation;

			for (size_t j = 0; j < world::s_world->entities.size(); j++)
			{
				const world::Entity &targetEntity = world::s_world->entities[j];
				const char *targetName = targetEntity.findValue("targetname");

				if (targetName && !util::Stricmp(targetName, target))
				{
					const char *origin = targetEntity.findValue("origin");

					if (!origin)
						continue;

					sscanf(origin, "%f %f %f", &light.targetPosition.x, &light.targetPosition.y, &light.targetPosition.z);
					break;
				}
			}
		}

		CalculateLightEnvelope(&light);
		s_lightBaker->lights.push_back(light);
	}
}

static vec3 BakeEntityLights(vec3 samplePosition, vec3 sampleNormal)
{
	vec3 accumulatedLight;

	for (StaticLight &light : s_lightBaker->lights)
	{
		float totalAttenuation = 0;

		for (int si = 0; si < s_lightBaker->nSamples; si++)
		{
			vec3 dir(light.position + s_lightBaker->posJitter[si] - samplePosition); // Jitter light position.
			const vec3 displacement(dir);
			float distance = dir.normalize();

			if (distance >= light.envelope)
				continue;

			distance = std::max(distance, 16.0f); // clamp the distance to prevent super hot spots
			float attenuation = 0;

			if (light.flags & StaticLightFlags::LinearAttenuation)
			{
				//attenuation = 1.0f - distance / light.intensity;
				attenuation = std::max(0.0f, light.photons * s_lightBaker->linearScale - distance);
			}
			else
			{
				// Inverse distance-squared attenuation.
				attenuation = light.photons / (distance * distance);
			}
						
			if (attenuation <= 0)
				continue;

			if (light.flags & StaticLightFlags::AngleAttenuation)
			{
				attenuation *= vec3::dotProduct(sampleNormal, dir);

				if (attenuation <= 0)
					continue;
			}

			if (light.flags & StaticLightFlags::Spotlight)
			{
				vec3 normal(light.targetPosition - (light.position + s_lightBaker->posJitter[si]));
				float coneLength = normal.normalize();

				if (coneLength <= 0)
					coneLength = 64;

				const float radiusByDist = (light.radius + 16) / coneLength;
				const float distByNormal = -vec3::dotProduct(displacement, normal);

				if (distByNormal < 0)
					continue;
							
				const vec3 pointAtDist(light.position + s_lightBaker->posJitter[si] + normal * distByNormal);
				const float radiusAtDist = radiusByDist * distByNormal;
				const vec3 distToSample(samplePosition - pointAtDist);
				const float sampleRadius = distToSample.length();

				// outside the cone
				if (sampleRadius >= radiusAtDist)
					continue;

				if (sampleRadius > (radiusAtDist - 32.0f))
				{
					attenuation *= (radiusAtDist - sampleRadius) / 32.0f;
				}
			}

			RTCRay ray;
			const vec3 org(samplePosition + sampleNormal * 0.1f);
			ray.org[0] = org.x;
			ray.org[1] = org.y;
			ray.org[2] = org.z;
			ray.tnear = 0;
			ray.tfar = distance;
			ray.dir[0] = dir.x;
			ray.dir[1] = dir.y;
			ray.dir[2] = dir.z;
			ray.geomID = RTC_INVALID_GEOMETRY_ID;
			ray.primID = RTC_INVALID_GEOMETRY_ID;
			ray.mask = -1;
			ray.time = 0;

			embreeOccluded(s_lightBaker->embreeScene, ray);

			if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
				continue; // hit

			totalAttenuation += attenuation * (1.0f / s_lightBaker->nSamples);
		}
					
		if (totalAttenuation > 0)
			accumulatedLight += light.color.rgb() * totalAttenuation;
	}

	return accumulatedLight;
}

/*
================================================================================
AREA LIGHTS
================================================================================
*/

static void LoadAreaLightTextures()
{
	// Load area light surface textures into main memory.
	// Need to do this on the main thread.
	for (int mi = 0; mi < world::GetNumModels(); mi++)
	{
		for (int si = 0; si < world::GetNumSurfaces(mi); si++)
		{
			const world::Surface &surface = world::GetSurface(mi, si);

			if (surface.material->surfaceLight <= 0)
				continue;

			// Check cache.
			AreaLightTexture *texture = nullptr;

			for (AreaLightTexture &alt : s_lightBaker->areaLightTextures)
			{
				if (alt.material == surface.material)
				{
					texture = &alt;
					break;
				}
			}

			if (texture)
				continue; // In cache.

			// Grab the first valid texture for now.
			const char *filename = nullptr;

			for (size_t i = 0; i < Material::maxStages; i++)
			{
				const MaterialStage &stage = surface.material->stages[i];

				if (stage.active)
				{
					const char *texture = stage.bundles[MaterialTextureBundleIndex::DiffuseMap].textures[0]->getName();

					if (texture[0] != '*') // Ignore special textures, e.g. lightmaps.
					{
						filename = texture;
						break;
					}
				}
			}

			if (!filename)
			{
				interface::PrintWarningf("Error finding area light texture for material %s\n", surface.material->name);
				continue;
			}

			Image image = LoadImage(filename);

			if (!image.data)
			{
				interface::PrintWarningf("Error loading area light image %s from material %s\n", filename, surface.material->name);
				continue;
			}

			// Downscale the image.
			s_lightBaker->areaLightTextures.push_back(AreaLightTexture());
			texture = &s_lightBaker->areaLightTextures[s_lightBaker->areaLightTextures.size() - 1];
			texture->material = surface.material;
			texture->width = texture->height = 32;
			texture->nComponents = image.nComponents;
			texture->data.resize(texture->width * texture->height * texture->nComponents);
			stbir_resize_uint8(image.data, image.width, image.height, 0, texture->data.data(), texture->width, texture->height, 0, image.nComponents);
			image.release(image.data, nullptr);
#if 0
			stbi_write_tga(util::GetFilename(filename), texture->width, texture->height, texture->nComponents, texture->data.data());
#endif
		}
	}
}

static const AreaLightTexture *FindAreaLightTexture(const Material *material)
{
	for (const AreaLightTexture &alt : s_lightBaker->areaLightTextures)
	{
		if (alt.material == material)
			return &alt;
	}

	return nullptr;
}

static void CreateAreaLights()
{
	// Calculate normalized/average colors.
	for (AreaLightTexture &texture : s_lightBaker->areaLightTextures)
	{
		// Calculate colors from the texture.
		// See q3map2 LoadShaderImages
		vec4 color;

		for (int i = 0; i < texture.width * texture.height; i++)
		{
			const uint8_t *c = &texture.data[i * texture.nComponents];
			color += vec4(c[0], c[1], c[2], c[3]);
		}

		const float max = std::max(color.r, std::max(color.g, color.b));

		if (max == 0)
		{
			texture.normalizedColor = vec4::white;
		}
		else
		{
			texture.normalizedColor = vec4(color.xyz() / max, 1.0f);
		}

		texture.averageColor = color / float(texture.width * texture.height);
	}

	// Create area lights.
	s_areaLights.clear();

	for (int mi = 0; mi < world::GetNumModels(); mi++)
	{
		for (int si = 0; si < world::GetNumSurfaces(mi); si++)
		{
			const world::Surface &surface = world::GetSurface(mi, si);

			if (surface.material->surfaceLight <= 0)
				continue;

			// Handle sky in the indirect pass(es).
			if (surface.material->isSky || (surface.flags & SURF_SKY))
				continue;

			const AreaLightTexture *texture = FindAreaLightTexture(surface.material);

			if (!texture)
				continue; // A warning will have been printed when the image failed to load.

			// autosprite materials become point lights.
			if (surface.material->hasAutoSpriteDeform())
			{
				StaticLight light;
				light.color = texture ? texture->normalizedColor : vec4::white;
				light.flags = StaticLightFlags::DefaultMask;
				light.photons = surface.material->surfaceLight * s_lightBaker->pointScale;
				light.position = surface.cullinfo.bounds.midpoint();
				CalculateLightEnvelope(&light);
				s_lightBaker->lights.push_back(light);
				continue;
			}

			AreaLight light;
			light.modelIndex = mi;
			light.surfaceIndex = si;
			light.texture = texture;
			const std::vector<Vertex> &vertices = world::GetVertexBuffer(surface.bufferIndex);

			// Create one sample per triangle at the midpoint.
			for (size_t i = 0; i < surface.indices.size(); i += 3)
			{
				const Vertex *v[3];
				v[0] = &vertices[surface.indices[i + 0]];
				v[1] = &vertices[surface.indices[i + 1]];
				v[2] = &vertices[surface.indices[i + 2]];

				// From q3map2 RadSubdivideDiffuseLight
				const float area = vec3::crossProduct(v[1]->pos - v[0]->pos, v[2]->pos - v[0]->pos).length();

				if (area < 1.0f)
					continue;

				AreaLightSample sample;
				sample.position = (v[0]->pos + v[1]->pos + v[2]->pos) / 3.0f;
				const vec3 normal((v[0]->normal + v[1]->normal + v[2]->normal) / 3.0f);
				sample.position += normal * 0.1f; // push out a little
				sample.texCoord = (v[0]->texCoord + v[1]->texCoord + v[2]->texCoord) / 3.0f;
				//sample.photons = surface.material->surfaceLight * area * areaScale;
				sample.photons = surface.material->surfaceLight * s_lightBaker->formFactorValueScale * s_lightBaker->areaScale;
				sample.winding.numpoints = 3;
				sample.winding.p[0] = v[0]->pos;
				sample.winding.p[1] = v[1]->pos;
				sample.winding.p[2] = v[2]->pos;
				light.samples.push_back(std::move(sample));
			}

			s_areaLights.push_back(std::move(light));
		}
	}

	// Use the world PVS - leaf cluster to leaf cluster visibility - to precompute leaf cluster to area light visibility.
	if (!s_areaLights.empty() || !world::s_world->visData)
		return;

	s_lightBaker->areaLightClusterBytes = (int)std::ceil(s_areaLights.size() / 8.0f); // Need 1 bit per area light.
	s_lightBaker->areaLightVisData.resize(world::s_world->nClusters * s_lightBaker->areaLightClusterBytes);
	memset(s_lightBaker->areaLightVisData.data(), 0, s_lightBaker->areaLightVisData.size());

	for (int i = 0; i < world::s_world->nClusters; i++)
	{
		const uint8_t *worldPvs = &world::s_world->visData[i * world::s_world->clusterBytes];
		uint8_t *areaLightPvs = &s_lightBaker->areaLightVisData[i * s_lightBaker->areaLightClusterBytes];

		for (size_t j = world::s_world->firstLeaf; j < world::s_world->nodes.size(); j++)
		{
			world::Node &leaf = world::s_world->nodes[j];

			if (!(worldPvs[leaf.cluster >> 3] & (1 << (leaf.cluster & 7))))
				continue;

			for (int k = 0; k < leaf.nSurfaces; k++)
			{
				const int surfaceIndex = world::s_world->leafSurfaces[leaf.firstSurface + k];

				// If this surface is an area light, set the appropriate bit.
				for (size_t l = 0; l < s_areaLights.size(); l++)
				{
					if (s_areaLights[l].surfaceIndex == surfaceIndex)
					{
						areaLightPvs[l >> 3] |= (1 << (l & 7));
						break;
					}
				}
			}
		}
	}
}

/*
   PointToPolygonFormFactor()
   calculates the area over a point/normal hemisphere a winding covers
   ydnar: fixme: there has to be a faster way to calculate this
   without the expensive per-vert sqrts and transcendental functions
   ydnar 2002-09-30: added -faster switch because only 19% deviance > 10%
   between this and the approximation
 */

#define ONE_OVER_2PI    0.159154942f    //% (1.0f / (2.0f * 3.141592657f))

static float PointToPolygonFormFactor(vec3 point, vec3 normal, const Winding &w)
{
	/* this is expensive */
	vec3 dirs[Winding::maxPoints];

	for (int i = 0; i < w.numpoints; i++)
	{
		dirs[i] = (w.p[i] - point).normal();
	}

	/* duplicate first vertex to avoid mod operation */
	dirs[w.numpoints] = dirs[0];

	/* calculcate relative area */
	float total = 0;

	for (int i = 0; i < w.numpoints; i++)
	{
		/* get a triangle */
		float dot = vec3::dotProduct(dirs[i], dirs[i + 1]);

		/* roundoff can cause slight creep, which gives an IND from acos */
		if ( dot > 1.0f ) {
			dot = 1.0f;
		}
		else if ( dot < -1.0f ) {
			dot = -1.0f;
		}

		/* get the angle */
		const float angle = acos(dot);

		vec3 triNormal = vec3::crossProduct(dirs[i], dirs[i + 1]);

		if (triNormal.normalize() < 0.0001f)
			continue;

		const float facing = vec3::dotProduct(normal, triNormal);
		total += facing * angle;

		/* ydnar: this was throwing too many errors with radiosity + crappy maps. ignoring it. */
		if ( total > 6.3f || total < -6.3f ) {
			return 0.0f;
		}
	}

	/* now in the range of 0 to 1 over the entire incoming hemisphere */
	//%	total /= (2.0f * 3.141592657f);
	total *= ONE_OVER_2PI;
	return total;
}

static vec3 BakeAreaLights(vec3 samplePosition, vec3 sampleNormal)
{
	world::Node *sampleLeaf = world::LeafFromPosition(samplePosition);
	vec3 accumulatedLight;

	for (size_t i = 0; i < s_areaLights.size(); i++)
	{
		// Skip PVS check if outside map or there's no vis data.
		if (sampleLeaf->cluster != -1 && !s_lightBaker->areaLightVisData.empty())
		{
			const uint8_t *pvs = &s_lightBaker->areaLightVisData[sampleLeaf->cluster * s_lightBaker->areaLightClusterBytes];

			if (!(pvs[i >> 3] & (1 << (i & 7))))
				continue;
		}

		const AreaLight &areaLight = s_areaLights[i];

		for (const AreaLightSample &areaLightSample : areaLight.samples)
		{
			vec3 dir(areaLightSample.position - samplePosition);
			const float distance = dir.normalize();

			// Check if light is behind the sample point.
			// Ignore two-sided surfaces.
			float angle = vec3::dotProduct(sampleNormal, dir);
							
			if (areaLight.texture->material->cullType != MaterialCullType::TwoSided && angle <= 0)
				continue;

			// Faster to trace the ray before PTPFF.
			RTCRay ray;
			const vec3 org(samplePosition + sampleNormal * 0.1f);
			ray.org[0] = org.x;
			ray.org[1] = org.y;
			ray.org[2] = org.z;
			ray.tnear = 0;
			ray.tfar = distance * 0.9f; // FIXME: should check for exact hit instead?
			ray.dir[0] = dir.x;
			ray.dir[1] = dir.y;
			ray.dir[2] = dir.z;
			ray.geomID = RTC_INVALID_GEOMETRY_ID;
			ray.primID = RTC_INVALID_GEOMETRY_ID;
			ray.mask = -1;
			ray.time = 0;

			embreeOccluded(s_lightBaker->embreeScene, ray);

			if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
				continue; // hit

			float factor = PointToPolygonFormFactor(samplePosition, sampleNormal, areaLightSample.winding);

			if (areaLight.texture->material->cullType == MaterialCullType::TwoSided)
				factor = fabs(factor);

			if (factor <= 0)
				continue;

			accumulatedLight += areaLight.texture->normalizedColor.rgb() * areaLightSample.photons * factor;
		}
	}

	return accumulatedLight;
}

/*
================================================================================
HEMICUBES / INDIRECT LIGHTING
================================================================================
*/

static void InitializeHemicubeRendering()
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

	if (s_lightBaker->currentIndirectPass == 0)
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
		s_lightBaker->hemicubeWeightsTexture = Texture::create("*hemicube_weights", image, 0, bgfx::TextureFormat::RG32F);

		// Misc. state.
	#ifdef DEBUG_HEMICUBE_RENDERING
		s_lightBaker->hemicubeBatchData.resize(s_lightBaker->hemicubeBatchSize.x * s_lightBaker->hemicubeBatchSize.y * 4);
		s_lightBaker->hemicubeWeightedBatchData.resize(s_lightBaker->hemicubeDownsampleSize.x * s_lightBaker->hemicubeDownsampleSize.y * 4);
	#endif
		s_lightBaker->hemicubeIntegrationData.resize(s_lightBaker->nHemicubesInBatch.x * s_lightBaker->nHemicubesInBatch.y * 4);
		s_lightBaker->hemicubeBatchLocations.resize(s_lightBaker->nHemicubesInBatch.x * s_lightBaker->nHemicubesInBatch.y);
	}

	s_lightBaker->nLuxelsProcessed = 0;
	s_lightBaker->indirectLightProgress = 0;
	s_lightBaker->nHemicubesToRenderPerFrame = 8;
	s_lightBaker->nHemicubesRenderedInBatch = 0;
	s_lightBaker->nHemicubeBatchesProcessed = 0;
	s_lightBaker->finishedHemicubeBatch = false;
	s_lightBaker->finishedBakingIndirect = false;
	s_lightBaker->currentLightmapIndex = 0;
	s_lightBaker->currentLuxelIndex = 0;
}

static uint32_t AsyncReadTexture(bgfx::FrameBufferHandle source, bgfx::TextureHandle dest, void *destData, uint16_t width, uint16_t height)
{
	const uint8_t viewId = main::PushView(main::s_main->defaultFb, BGFX_CLEAR_NONE, mat4::empty, mat4::empty, Rect());
	bgfx::blit(viewId, dest, 0, 0, source, 0, 0, 0, width, height);
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
	bgfx::setTexture(0, s_lightBakerPersistent->hemicubeAtlasSampler.handle, s_lightBakerPersistent->hemicubeFb[fbRead].handle, 0);
	bgfx::setTexture(1, s_lightBakerPersistent->hemicubeWeightsSampler.handle, s_lightBaker->hemicubeWeightsTexture->getHandle());
	s_lightBakerPersistent->hemicubeWeightsTextureSizeUniform.set(vec4((float)s_lightBaker->hemicubeSize.x, (float)s_lightBaker->hemicubeSize.y, 0, 0));
	main::RenderScreenSpaceQuad(s_lightBakerPersistent->hemicubeFb[fbWrite], main::ShaderProgramId::HemicubeWeightedDownsample, BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE, BGFX_CLEAR_COLOR, main::s_main->isTextureOriginBottomLeft, Rect(0, 0, s_lightBaker->hemicubeDownsampleSize.x, s_lightBaker->hemicubeDownsampleSize.y));

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
		bgfx::setTexture(0, s_lightBakerPersistent->hemicubeAtlasSampler.handle, s_lightBakerPersistent->hemicubeFb[fbRead].handle, 0);
		main::RenderScreenSpaceQuad(s_lightBakerPersistent->hemicubeFb[fbWrite], main::ShaderProgramId::HemicubeDownsample, BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE, BGFX_CLEAR_COLOR, main::s_main->isTextureOriginBottomLeft, Rect(0, 0, outHemiSize * s_lightBaker->nHemicubesInBatch.x, outHemiSize * s_lightBaker->nHemicubesInBatch.y));
	}

	// Start async texture read of integrated data.
	return AsyncReadTexture(s_lightBakerPersistent->hemicubeFb[fbWrite].handle, s_lightBakerPersistent->hemicubeIntegrationReadTexture, integrationData, s_lightBaker->nHemicubesInBatch.x, s_lightBaker->nHemicubesInBatch.y);
}

static bool BakeIndirectLight(uint32_t frameNo)
{
	// Update frame timing and add to history.
	const int frameMs = int((bx::getHPCounter() - s_lightBaker->lastFrameTime) * (1000.0f / (float)bx::getHPFrequency()));
	s_lightBaker->lastFrameTime = bx::getHPCounter();
	s_lightBaker->frameMsHistory[s_lightBaker->frameMsHistoryIndex++] = frameMs;

	if (s_lightBaker->frameMsHistoryIndex > (int)s_lightBaker->frameMsHistory.size())
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
			Lightmap &lightmap = s_lightBaker->lightmaps[s_lightBaker->currentLightmapIndex];
			Luxel &luxel = lightmap.luxels[s_lightBaker->currentLuxelIndex];
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

			main::RenderHemicube(s_lightBakerPersistent->hemicubeFb[0], luxel.position + luxel.normal * 1.0f, luxel.normal, up, rectOffset, s_lightBaker->hemicubeFaceSize);
			s_lightBaker->hemicubeBatchLocations[s_lightBaker->nHemicubesRenderedInBatch].lightmap = &lightmap;
			s_lightBaker->hemicubeBatchLocations[s_lightBaker->nHemicubesRenderedInBatch].luxel = &luxel;
			s_lightBaker->nLuxelsProcessed++;
			s_lightBaker->nHemicubesRenderedInBatch++;
			s_lightBaker->currentLuxelIndex++;

			if (s_lightBaker->nHemicubesRenderedInBatch >= s_lightBaker->nHemicubesInBatch.x * s_lightBaker->nHemicubesInBatch.y)
			{
				s_lightBaker->finishedHemicubeBatch = true;
			}

			if (s_lightBaker->currentLuxelIndex >= (int)lightmap.luxels.size())
			{
				// Processed all luxels in this lightmap, advance to the next one.
				s_lightBaker->currentLightmapIndex++;
				s_lightBaker->currentLuxelIndex = 0;

				if (s_lightBaker->currentLightmapIndex >= (int)s_lightBaker->lightmaps.size())
				{
					// Processed all lightmaps.
					s_lightBaker->finishedHemicubeBatch = true;
					s_lightBaker->finishedBakingIndirect = true;
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
				auto rgba = (const vec4 *)&s_lightBaker->hemicubeIntegrationData[offset * 4];
				HemicubeLocation &hemicube = s_lightBaker->hemicubeBatchLocations[offset];
				hemicube.lightmap->color[hemicube.luxel->offset] += *rgba * 255.0f * s_lightBaker->indirectLightScale / float(s_lightBaker->currentIndirectPass + 1);
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
	const int progress = int(s_lightBaker->nLuxelsProcessed / (float)s_lightBaker->totalLuxels * 100.0f);

	if (progress != s_lightBaker->indirectLightProgress)
	{
		s_lightBaker->indirectLightProgress = progress;
		SetStatus(LightBaker::Status::BakingIndirectLight_Running, progress);
	}

	return true;
}

/*
================================================================================
LIGHTMAP ENCODING
================================================================================
*/

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

static void EncodeLightmaps()
{
	const vec2i lightmapSize = world::GetLightmapSize();

	// Dilate lightmaps, then encode to RGBM.
#define DILATE
#ifdef DILATE
	std::vector<vec4> dilatedLightmap;
	dilatedLightmap.resize(lightmapSize.x * lightmapSize.y);
#endif

	for (Lightmap &lightmap : s_lightBaker->lightmaps)
	{
#ifdef DILATE
		lmImageDilate(&lightmap.color[0].x, &dilatedLightmap[0].x, lightmapSize.x, lightmapSize.y, 4);
#endif

		for (int i = 0; i < lightmapSize.x * lightmapSize.y; i++)
		{
#ifdef DILATE
			const vec4 &src = dilatedLightmap[i];
#else
			const vec4 &src = lightmap.color[i];
#endif
			uint8_t *dest = &lightmap.colorBytes[i * 4];
			// Colors are floats, but in 0-255+ range.
			util::EncodeRGBM(src.rgb() * g_overbrightFactor / 255.0f).toBytes(dest);
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
	int progress = 0;

	// Count surface triangles for prealloc.
	// Non-0 world models (e.g. doors) may be lightmapped, but don't occlude.
	int totalOccluderTriangles = 0;

	for (int mi = 0; mi < world::GetNumModels(); mi++)
	{
		for (int si = 0; si < world::GetNumSurfaces(mi); si++)
		{
			const world::Surface &surface = world::GetSurface(mi, si);

			if (mi == 0 && DoesSurfaceOccludeLight(surface))
			{
				totalOccluderTriangles += (int)surface.indices.size() / 3;
			}
		}
	}

	// Count total vertices.
	int totalVertices = 0;

	for (int i = 0; i < world::GetNumVertexBuffers(); i++)
	{
		totalVertices += (int)world::GetVertexBuffer(i).size();
	}

	// Setup embree.
	if (!CreateEmbreeGeometry(totalVertices, totalOccluderTriangles))
	{
		SetStatus(LightBaker::Status::Error);
		return 0;
	}

	// Allocate lightmap memory.
	const vec2i lightmapSize = world::GetLightmapSize();
	s_lightBaker->lightmaps.resize(world::GetNumLightmaps());

	for (Lightmap &lightmap : s_lightBaker->lightmaps)
	{
		lightmap.color.resize(lightmapSize.x * lightmapSize.y);
		lightmap.colorBytes.resize(lightmapSize.x * lightmapSize.y * 4);
	}

	// Rasterize.
	RasterizeLightmappedSurfaces();

	// Setup jitter.
	srand(1);

	for (int si = 1; si < s_lightBaker->nSamples; si++) // index 0 left as (0, 0, 0)
	{
		const vec2 dirJitterRange(-0.1f, 0.1f);
		const vec2 posJitterRange(-2.0f, 2.0f);

		for (int i = 0; i < 3; i++)
		{
			s_lightBaker->dirJitter[si][i] = math::RandomFloat(dirJitterRange.x, dirJitterRange.y);
			s_lightBaker->posJitter[si][i] = math::RandomFloat(posJitterRange.x, posJitterRange.y);
		}
	}

	// Setup lights.
	CreateAreaLights();
	CreateEntityLights();

	// Bake direct lighting.
	int64_t startTime = bx::getHPCounter();
	s_lightBaker->nLuxelsProcessed = 0;
	const SunLight &sunLight = main::GetSunLight();
	const float maxRayLength = world::GetBounds().toRadius() * 2; // World bounding sphere circumference.

	for (Lightmap &lightmap : s_lightBaker->lightmaps)
	{
		for (Luxel &luxel : lightmap.luxels)
		{
			vec4 &luxelColor = lightmap.color[luxel.offset];
			luxelColor = s_lightBaker->ambientLight;
			luxelColor += BakeAreaLights(luxel.position, luxel.normal);
			luxelColor += BakeEntityLights(luxel.position, luxel.normal);

	#if 1
			// Sunlight.
			float totalAttenuation = 0;

			for (int si = 0; si < s_lightBaker->nSamples; si++)
			{
				const vec3 dir(sunLight.direction + s_lightBaker->dirJitter[si]); // Jitter light direction.
				const float attenuation = vec3::dotProduct(luxel.normal, dir) * (1.0f / s_lightBaker->nSamples);

				if (attenuation < 0)
					continue;

				RTCRay ray;
				const vec3 org(luxel.position + luxel.normal * 0.1f); // push out from the surface a little
				ray.org[0] = org.x;
				ray.org[1] = org.y;
				ray.org[2] = org.z;
				ray.tnear = 0;
				ray.tfar = maxRayLength;
				ray.dir[0] = dir.x;
				ray.dir[1] = dir.y;
				ray.dir[2] = dir.z;
				ray.geomID = RTC_INVALID_GEOMETRY_ID;
				ray.primID = RTC_INVALID_GEOMETRY_ID;
				ray.mask = -1;
				ray.time = 0;
				embreeIntersect(s_lightBaker->embreeScene, ray);

				if (ray.geomID != RTC_INVALID_GEOMETRY_ID && (s_lightBaker->faceFlags[ray.primID] & FaceFlags::Sky))
				{
					totalAttenuation += attenuation;
				}
			}

			if (totalAttenuation > 0)
				luxelColor += sunLight.light * totalAttenuation * 255.0;
	#endif

			s_lightBaker->nLuxelsProcessed++;

			// Update progress.
			const int newProgress = int(s_lightBaker->nLuxelsProcessed / (float)s_lightBaker->totalLuxels * 100.0f);

			if (newProgress != progress)
			{
				// Check for cancelling.
				if (GetStatus() == LightBaker::Status::Cancelled)
					return 1;

				progress = newProgress;
				SetStatus(LightBaker::Status::BakingDirectLight, progress);
			}
		}
	}

	s_lightBaker->directBakeTime = bx::getHPCounter() - startTime;

	// Update the lightmaps after the direct lighting pass has finished. Indirect lighting needs to render using the updated lightmaps.
	if (!ThreadUpdateLightmaps())
		return 1;

	// Ready for indirect lighting, which happens in the main thread. Wait until it finishes.
	for (int i = 0; i < s_lightBaker->nIndirectPasses; i++)
	{
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

	if (!LoadEmbreeLibrary())
		return;

	s_lightBaker = std::make_unique<LightBaker>();
	s_lightBakerPersistent = std::make_unique<LightBakerPersistent>();
	s_lightBaker->nSamples = math::Clamped(nSamples, 1, (int)LightBaker::maxSamples);
	LoadAreaLightTextures();
	s_lightBaker->startTime = bx::getHPCounter();
	s_lightBaker->mutex = SDL_CreateMutex();

	if (!s_lightBaker->mutex)
	{
		interface::PrintWarningf("Creating light baker mutex failed. Reason: \"%s\"", SDL_GetError());
		s_lightBaker.reset();
		return;
	}

	s_lightBaker->thread = SDL_CreateThread(Thread, "LightBaker", nullptr);

	if (!s_lightBaker->thread)
	{
		interface::PrintWarningf("Creating light baker thread failed. Reason: \"%s\"", SDL_GetError());
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

	DestroyEmbreeGeometry();
	s_lightBaker.reset();
}

void Shutdown()
{
	if (!s_lightBakerPersistent.get())
		return;

	if (bgfx::isValid(s_lightBakerPersistent->hemicubeIntegrationReadTexture))
	{
		bgfx::destroyTexture(s_lightBakerPersistent->hemicubeIntegrationReadTexture);
	}

	s_lightBakerPersistent.reset();
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
		InitializeHemicubeRendering();

		if (s_lightBaker->currentIndirectPass == 0)
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
			if (s_lightBaker->currentIndirectPass == s_lightBaker->nIndirectPasses - 1)
			{
				s_lightBaker->indirectBakeTime = bx::getHPCounter() - s_lightBaker->indirectBakeStartTime;
			}

			s_lightBaker->currentIndirectPass++;
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
				texture->update(bgfx::makeRef(lightmap.colorBytes.data(), (uint32_t)lightmap.colorBytes.size()), 0, 0, lightmapSize.x, lightmapSize.y);
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
