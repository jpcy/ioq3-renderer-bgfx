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
#include "Precompiled.h"
#pragma hdrstop

#if defined(USE_LIGHT_BAKER)
#include "LightBaker.h"
#include "World.h"

#include "stb_image_resize.h"

namespace renderer {
namespace light_baker {

static bool DoesSurfaceOccludeLight(const world::Surface &surface)
{
	// Translucent surfaces (e.g. flames) shouldn't occlude.
	// Not handling alpha-testing yet.
	return surface.type != world::SurfaceType::Ignore && surface.type != world::SurfaceType::Flare && (surface.contentFlags & CONTENTS_TRANSLUCENT) == 0 && (surface.flags & SURF_ALPHASHADOW) == 0;
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

#define EMBREE_FUNCTION(func, type, name) func = (type)SDL_LoadFunction(s_lightBaker->embreeLibrary, name); if (!func) { SDL_UnloadObject(s_lightBaker->embreeLibrary); interface::PrintWarningf("Error loading Embree function %s\n", name); return false; }

bool InitializeEmbree()
{
	s_lightBaker->embreeLibrary = SDL_LoadObject(EMBREE_LIB);

	if (!s_lightBaker->embreeLibrary)
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

void ShutdownEmbree()
{
	if (s_lightBaker->embreeScene)
		embreeDeleteScene(s_lightBaker->embreeScene);

	if (s_lightBaker->embreeDevice)
		embreeDeleteDevice(s_lightBaker->embreeDevice);

	if (s_lightBaker->embreeLibrary)
		SDL_UnloadObject(s_lightBaker->embreeLibrary);
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

		for (int i = 0; i < (int)surface.bufferIndex; i++)
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

void LoadAreaLightTextures()
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
			const std::vector<Vertex> &vertices = world::GetVertexBuffer((int)surface.bufferIndex);

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
				const vec3 normal((v[0]->getNormal() + v[1]->getNormal() + v[2]->getNormal()) / 3.0f);
				sample.position += normal * 0.1f; // push out a little
				sample.texCoord = (v[0]->getTexCoord().xy() + v[1]->getTexCoord().xy() + v[2]->getTexCoord().xy()) / 3.0f;
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

bool InitializeDirectLight()
{
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
		return false;
	}

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

	// Create lights.
	CreateAreaLights();
	CreateEntityLights();
	return true;
}

bool BakeDirectLight()
{
	int progress = 0;
	int64_t startTime = bx::getHPCounter();
	const SunLight &sunLight = main::GetSunLight();
	const float maxRayLength = world::GetBounds().toRadius() * 2; // World bounding sphere circumference.
	InitializeRasterization(0, 0);
	s_lightBaker->totalLuxels = 0;

	for (;;)
	{
		Luxel luxel = RasterizeLuxel();

		if (luxel.sentinel)
			break;

		s_lightBaker->totalLuxels++;
		Lightmap &lightmap = s_lightBaker->lightmaps[luxel.lightmapIndex];
		vec3 &luxelColor = lightmap.passColor[luxel.offset];
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

		// Update progress.
		const int newProgress = int(GetNumRasterizedTriangles() / (float)s_lightBaker->totalLightmappedTriangles * 100.0f);

		if (newProgress != progress)
		{
			// Check for cancelling.
			if (GetStatus() == LightBaker::Status::Cancelled)
				return false;

			progress = newProgress;
			SetStatus(LightBaker::Status::BakingDirectLight, progress);
		}
	}

	s_lightBaker->directBakeTime = bx::getHPCounter() - startTime;
	return true;
}
	
} // namespace light_baker
} // namespace renderer
#endif // USE_LIGHT_BAKER
