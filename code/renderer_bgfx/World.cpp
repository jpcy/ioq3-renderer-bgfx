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
#include "World.h"

namespace renderer {
namespace world {
	
std::unique_ptr<World> s_world;
static const int MAX_VERTS_ON_POLY = 64;

static vec2 AtlasTexCoord(vec2 uv, int index, vec2i lightmapAtlasSize)
{
	const int tileX = index % lightmapAtlasSize.x;
	const int tileY = index / lightmapAtlasSize.x;
	vec2 result;
	result.u = (tileX / (float)lightmapAtlasSize.x) + (uv.u / (float)lightmapAtlasSize.x);
	result.v = (tileY / (float)lightmapAtlasSize.y) + (uv.v / (float)lightmapAtlasSize.y);
	return result;
}

static bool SurfaceCompare(const Surface *s1, const Surface *s2)
{
	if (s1->material->index < s2->material->index)
	{
		return true;
	}
	else if (s1->material->index == s2->material->index)
	{
		if (s1->fogIndex < s2->fogIndex)
		{
			return true;
		}
		else if (s1->fogIndex == s2->fogIndex)
		{
			if (s1->bufferIndex < s2->bufferIndex)
				return true;
		}
	}

	return false;
}

static bool IgnoreSurface(const Surface &surface)
{
	return surface.type == SurfaceType::Ignore || surface.type == SurfaceType::Flare;
}

class WorldModel : public Model
{
public:
	WorldModel(int index) : index_(index)
	{
		util::Sprintf(name_, sizeof(name_), "*%d", index_);
	}

	bool load(const ReadOnlyFile &file) override
	{
		return true;
	}

	Bounds getBounds() const override
	{
		return s_world->modelDefs[index_].bounds;
	}

	Material *getMaterial(size_t surfaceNo) const override
	{
		const ModelDef &def = s_world->modelDefs[index_];

		// if it's out of range, return the first surface
		if (surfaceNo >= (int)def.nSurfaces)
			surfaceNo = 0;

		const Surface &surface = s_world->surfaces[def.firstSurface + surfaceNo];

		if (surface.material->lightmapIndex > MaterialLightmapId::None)
		{
			bool mipRawImage = true;

			// Get mipmap info for original texture.
			Texture *texture = g_textureCache->get(surface.material->name);

			if (texture)
				mipRawImage = (texture->getFlags() & TextureFlags::Mipmap) != 0;

			Material *mat = g_materialCache->findMaterial(surface.material->name, MaterialLightmapId::None, mipRawImage);
			mat->stages[0].rgbGen = MaterialColorGen::LightingDiffuse; // (SA) new
			return mat;
		}

		return surface.material;
	}

	bool isCulled(renderer::Entity *entity, const Frustum &cameraFrustum) const override
	{
		return cameraFrustum.clipBounds(getBounds(), mat4::transform(entity->rotation, entity->position)) == Frustum::ClipResult::Outside;
	}

	int lerpTag(const char *name, const renderer::Entity &entity, int startIndex, Transform *transform) const override
	{
		return -1;
	}

	void render(const mat3 &scenRotation, DrawCallList *drawCallList, renderer::Entity *entity) override
	{
		assert(drawCallList);
		assert(entity);
		const mat4 modelMatrix = mat4::transform(entity->rotation, entity->position);

		for (const BatchedSurface &surface : batchedSurfaces_)
		{
			DrawCall dc;
			dc.entity = entity;
			dc.flags = 0;
			dc.fogIndex = surface.fogIndex;
			dc.material = surface.material;
			dc.modelMatrix = modelMatrix;
			dc.vb.type = DrawCall::BufferType::Static;
			dc.vb.staticHandle = s_world->vertexBuffers[surface.bufferIndex].handle;
			dc.vb.nVertices = (uint32_t)s_world->vertices[surface.bufferIndex].size();
			dc.ib.type = DrawCall::BufferType::Static;
			dc.ib.staticHandle = indexBuffers_[surface.bufferIndex].handle;
			dc.ib.firstIndex = surface.firstIndex;
			dc.ib.nIndices = surface.nIndices;
			drawCallList->push_back(dc);
		}
	}

	void buildGeometry()
	{
		const ModelDef &def = s_world->modelDefs[index_];

		// Grab surfaces we aren't ignoring and sort them.
		std::vector<const Surface *> surfaces;

		for (size_t i = 0; i < def.nSurfaces; i++)
		{
			const Surface &surface = s_world->surfaces[def.firstSurface + i];

			if (!IgnoreSurface(surface))
				surfaces.push_back(&surface);
		}

		std::sort(surfaces.begin(), surfaces.end(), SurfaceCompare);

		// Batch surfaces.
		size_t firstSurface = 0;

		for (size_t i = 0; i < surfaces.size(); i++)
		{
			const Surface *surface = surfaces[i];
			const bool isLast = i == surfaces.size() - 1;
			const Surface *nextSurface = isLast ? nullptr : surfaces[i + 1];

			// Create new batch on certain surface state changes.
			if (!nextSurface || nextSurface->material != surface->material || nextSurface->fogIndex != surface->fogIndex || nextSurface->bufferIndex != surface->bufferIndex)
			{
				BatchedSurface bs;
				bs.fogIndex = surface->fogIndex;
				bs.material = surface->material;

				// Grab the indices for all surfaces in this batch.
				bs.bufferIndex = surface->bufferIndex;
				std::vector<uint16_t> &indices = indices_[bs.bufferIndex];
				bs.firstIndex = (uint32_t)indices.size();
				bs.nIndices = 0;

				for (size_t j = firstSurface; j <= i; j++)
				{
					const Surface *s = surfaces[j];
					const size_t copyIndex = indices.size();
					indices.resize(indices.size() + s->indices.size());
					memcpy(&indices[copyIndex], &s->indices[0], s->indices.size() * sizeof(uint16_t));
					bs.nIndices += (uint32_t)s->indices.size();
				}

				batchedSurfaces_.push_back(bs);
				firstSurface = i + 1;
			}
		}

		// Create static index buffers.
		for (size_t i = 0; i < s_world->currentGeometryBuffer + 1; i++)
		{
			IndexBuffer &ib = indexBuffers_[i];
			std::vector<uint16_t> &indices = indices_[i];

			if (indices.empty())
				continue;

			ib.handle = bgfx::createIndexBuffer(bgfx::makeRef(indices.data(), uint32_t(indices.size() * sizeof(uint16_t))));
		}
	}

private:
	struct BatchedSurface
	{
		Material *material;
		int fogIndex;
		size_t bufferIndex;
		uint32_t firstIndex;
		uint32_t nIndices;
	};

	int index_;
	std::vector<BatchedSurface> batchedSurfaces_;
	std::vector<uint16_t> indices_[s_maxWorldGeometryBuffers];
	IndexBuffer indexBuffers_[s_maxWorldGeometryBuffers];
};

static Material *FindMaterial(int materialIndex, int lightmapIndex)
{
	if (materialIndex < 0 || materialIndex >= (int)s_world->materials.size())
	{
		interface::Error("%s: bad material index %i", s_world->name, materialIndex);
	}

	if (lightmapIndex > 0)
	{
		lightmapIndex /= s_world->nLightmapsPerAtlas;
	}

	Material *material = g_materialCache->findMaterial(s_world->materials[materialIndex].name, lightmapIndex, true);

	// If the material had errors, just use default material.
	if (!material)
		return g_materialCache->getDefaultMaterial();

	return material;
}

static void SetSurfaceGeometry(Surface *surface, const Vertex *vertices, int nVertices, const uint16_t *indices, size_t nIndices, int lightmapIndex)
{
	std::vector<Vertex> *bufferVertices = &s_world->vertices[s_world->currentGeometryBuffer];

	// Increment the current vertex buffer if the vertices won't fit.
	if (bufferVertices->size() + nVertices >= UINT16_MAX)
	{
		if (++s_world->currentGeometryBuffer == s_maxWorldGeometryBuffers)
			interface::Error("Not enough world vertex buffers");

		bufferVertices = &s_world->vertices[s_world->currentGeometryBuffer];
	}

	// Append the vertices into the current vertex buffer.
	auto startVertex = (const uint32_t)bufferVertices->size();
	bufferVertices->resize(bufferVertices->size() + nVertices);
	memcpy(&(*bufferVertices)[startVertex], vertices, nVertices * sizeof(Vertex));

	for (int i = 0; i < nVertices; i++)
	{
		Vertex *v = &(*bufferVertices)[startVertex + i];

		if (lightmapIndex >= 0 && !s_world->lightmapAtlases.empty())
		{
			vec4 texCoord = v->getTexCoord();
			const vec2 atlasTexCoord = AtlasTexCoord(vec2(texCoord.z, texCoord.w), lightmapIndex % s_world->nLightmapsPerAtlas, s_world->lightmapAtlasSize);
			v->setTexCoord(texCoord.x, texCoord.y, atlasTexCoord.x, atlasTexCoord.y);
		}
	}

	// The surface needs to know which vertex buffer to use.
	surface->bufferIndex = s_world->currentGeometryBuffer;

	// CPU deforms need to know which vertices to use.
	surface->firstVertex = startVertex;
	surface->nVertices = (uint32_t)nVertices;

	// Copy indices into the surface. Relative indices are made absolute.
	surface->indices.resize(nIndices);

	for (size_t i = 0; i < nIndices; i++)
	{
		surface->indices[i] = indices[i] + startVertex;
	}
}

static void ReleaseLightmapAtlasImage(void *data, void *userData)
{
	free(data);
}

static void CreateBatchedSurfaces(const std::vector<Surface *> &surfaces, std::vector<BatchedSurface> *batchedSurfaces, std::vector<uint16_t> *batchedIndices, std::vector<Vertex> *cpuDeformVertices, std::vector<uint16_t> *cpuDeformIndices)
{
	assert(batchedSurfaces);
	assert(batchedIndices);
	assert(cpuDeformVertices);
	assert(cpuDeformIndices);

	// Clear indices.
	for (size_t i = 0; i < s_world->currentGeometryBuffer + 1; i++)
	{
		batchedIndices[i].clear();
	}

	// Clear CPU deform geometry.
	cpuDeformIndices->clear();
	cpuDeformVertices->clear();

	// Create batched surfaces.
	batchedSurfaces->clear();
	size_t firstSurface = 0;

	for (size_t i = 0; i < surfaces.size(); i++)
	{
		Surface *surface = surfaces[i];
		const bool isLast = i == surfaces.size() - 1;
		Surface *nextSurface = isLast ? nullptr : surfaces[i + 1];

		// Create new batch on certain surface state changes.
		if (!nextSurface || nextSurface->material != surface->material || nextSurface->fogIndex != surface->fogIndex || nextSurface->bufferIndex != surface->bufferIndex)
		{
			if (IgnoreSurface(*surface))
			{
				// Skip this batch.
				firstSurface = i + 1;
				continue;
			}

			BatchedSurface bs;
			bs.contentFlags = surface->contentFlags;
			bs.fogIndex = surface->fogIndex;
			bs.material = surface->material;
			bs.surfaceFlags = surface->flags;

			// Merge all the surface bounds in the batch.
			bs.bounds.setupForAddingPoints();

			for (size_t j = firstSurface; j <= i; j++)
			{
				bs.bounds.addPoints(surfaces[j]->cullinfo.bounds);
			}

			if (bs.material->hasAutoSpriteDeform())
			{
				// Grab the geometry for all surfaces in this batch.
				// It will be copied into a transient buffer and then deformed every Render() call.
				bs.firstIndex = (uint32_t)cpuDeformIndices->size();
				bs.nIndices = 0;
				bs.firstVertex = (uint32_t)cpuDeformVertices->size();
				bs.nVertices = 0;

				for (size_t j = firstSurface; j <= i; j++)
				{
					Surface *s = surfaces[j];

					// Make room in destination.
					const size_t firstDestIndex = cpuDeformIndices->size();
					cpuDeformIndices->resize(cpuDeformIndices->size() + s->indices.size());
					const size_t firstDestVertex = cpuDeformVertices->size();
					cpuDeformVertices->resize(cpuDeformVertices->size() + s->nVertices);

					// Append geometry.
					memcpy(&(*cpuDeformVertices)[firstDestVertex], &s_world->vertices[surface->bufferIndex][s->firstVertex], sizeof(Vertex) * s->nVertices);

					for (size_t k = 0; k < s->indices.size(); k++)
					{
						// Make indices relative.
						(*cpuDeformIndices)[firstDestIndex + k] = uint16_t(s->indices[k] - s->firstVertex + bs.nVertices);
					}

					bs.nVertices += s->nVertices;
					bs.nIndices += (uint32_t)s->indices.size();
				}
			}
			else
			{
				// Grab the indices for all surfaces in this batch.
				// They will be used directly by a dynamic index buffer.
				bs.bufferIndex = surface->bufferIndex;
				std::vector<uint16_t> &indices = batchedIndices[bs.bufferIndex];
				bs.firstIndex = (uint32_t)indices.size();
				bs.nIndices = 0;

				for (size_t j = firstSurface; j <= i; j++)
				{
					Surface *s = surfaces[j];
					const size_t copyIndex = indices.size();
					indices.resize(indices.size() + s->indices.size());
					memcpy(&indices[copyIndex], &s->indices[0], s->indices.size() * sizeof(uint16_t));
					bs.nIndices += (uint32_t)s->indices.size();
				}
			}

			batchedSurfaces->push_back(bs);
			firstSurface = i + 1;
		}
	}
}

static void CreateOrAppendSkySurface(std::vector<SkySurface> &skySurfaces, const Surface &surface)
{
	SkySurface *skySurface = nullptr;

	for (SkySurface &ss : skySurfaces)
	{
		if (ss.material == surface.material)
		{
			skySurface = &ss;
			break;
		}
	}

	if (!skySurface)
	{
		skySurfaces.push_back(SkySurface());
		skySurface = &skySurfaces[skySurfaces.size() - 1];
		skySurface->material = surface.material;
	}

	const size_t startVertex = skySurface->vertices.size();
	skySurface->vertices.resize(skySurface->vertices.size() + surface.indices.size());

	for (size_t l = 0; l < surface.indices.size(); l++)
	{
		skySurface->vertices[startVertex + l] = s_world->vertices[surface.bufferIndex][surface.indices[l]];
	}
}

void Load(const char *name)
{
	s_world = std::make_unique<World>();
	util::Strncpyz(s_world->name, name, sizeof(s_world->name));
	util::Strncpyz(s_world->baseName, util::SkipPath(s_world->name), sizeof(s_world->baseName));
	util::StripExtension(s_world->baseName, s_world->baseName, sizeof(s_world->baseName));

	ReadOnlyFile file(s_world->name);

	if (!file.isValid())
	{
		interface::Error("%s not found", s_world->name);
		return;
	}

	const uint8_t *fileData = file.getData();

	// Header
	auto header = (dheader_t *)fileData;

	const int version = LittleLong(header->version);

	if (version != BSP_VERSION)
	{
		interface::Error("%s has wrong version number (%i should be %i)", s_world->name, version, BSP_VERSION);
		return;
	}

	// Swap all the lumps and validate sizes.
	const int lumpSizes[] =
	{
		0, // LUMP_ENTITIES
		sizeof(dshader_t), // LUMP_SHADERS
		sizeof(dplane_t), // LUMP_PLANES
		sizeof(dnode_t), // LUMP_NODES
		sizeof(dleaf_t), // LUMP_LEAFS
		sizeof(int), // LUMP_LEAFSURFACES
		0, // LUMP_LEAFBRUSHES
		sizeof(dmodel_t), // LUMP_MODELS
		sizeof(dbrush_t), // LUMP_BRUSHES
		sizeof(dbrushside_t), // LUMP_BRUSHSIDES
		sizeof(drawVert_t), // LUMP_DRAWVERTS
		sizeof(int), // LUMP_DRAWINDEXES
		sizeof(dfog_t), // LUMP_FOGS
		sizeof(dsurface_t), // LUMP_SURFACES
		0, // LUMP_LIGHTMAPS
		0, // LUMP_LIGHTGRID
		0, // LUMP_VISIBILITY
	};

	for (size_t i = 0; i < HEADER_LUMPS; i++)
	{
		lump_t &l = header->lumps[i];
		l.fileofs = LittleLong(l.fileofs);
		l.filelen = LittleLong(l.filelen);

		if (lumpSizes[i] != 0 && (l.filelen % lumpSizes[i]))
		interface::Error("%s: lump %d has bad size", s_world->name, (int)i);
	}

	// Entities
	lump_t &lump = header->lumps[LUMP_ENTITIES];

	// Store for reference by the cgame.
	s_world->entityString.resize(lump.filelen + 1);
	strcpy(s_world->entityString.data(), (char *)&fileData[lump.fileofs]);
	s_world->entityParsePoint = s_world->entityString.data();
		
	// Parse.
	char *p = s_world->entityString.data();
	bool parsingEntity = false;
	Entity entity;

	for (;;)
	{
		char *token = util::Parse(&p);

		if (!token[0])
			break; // End of entity string.

		if (*token == '{') // Start of entity definition.
		{
			if (parsingEntity)
			{
				interface::PrintWarningf("Stray '{' when parsing entity string\n");
				break;
			}

			parsingEntity = true;
			entity.nKvps = 0;
		}
		else if (*token == '}') // End of entity definition.
		{
			if (!parsingEntity)
			{
				interface::PrintWarningf("Stray '}' when parsing entity string\n");
				break;
			}

			parsingEntity = false;
			s_world->entities.push_back(entity);

			// Process entity.
			const char *classname = entity.findValue("classname", "");

			if (!util::Stricmp(classname, "worldspawn"))
			{
				// Check for a different light grid size.
				const char *gridsize = entity.findValue("gridsize");

				if (gridsize)
					sscanf(gridsize, "%f %f %f", &s_world->lightGridSize.x, &s_world->lightGridSize.y, &s_world->lightGridSize.z);
			}
		}
		else
		{
			// Parse KVP.
			if (entity.nKvps == entity.kvps.size())
			{
				interface::PrintWarningf("Exceeded max entity KVPs\n");
				break;
			}

			EntityKVP &kvp = entity.kvps[entity.nKvps];
			util::Strncpyz(kvp.key, token, sizeof(kvp.key));
			token = util::Parse(&p);

			if (!token[0])
			{
				interface::PrintWarningf("Empty KVP in entity string. Key is \"%s\"\n", kvp.key);
				break;
			}

			util::Strncpyz(kvp.value, token, sizeof(kvp.value));
			entity.nKvps++;
		}
	}

	// Planes
	// Needs to be loaded before fogs.
	auto filePlane = (const dplane_t *)(fileData + header->lumps[LUMP_PLANES].fileofs);
	s_world->planes.resize(header->lumps[LUMP_PLANES].filelen / sizeof(*filePlane));

	for (Plane &p : s_world->planes)
	{
		p = Plane(filePlane->normal, filePlane->dist);
		p.setupFastBoundsTest();
		filePlane++;
	}

	// Fogs
	auto fileFogs = (const dfog_t *)(fileData + header->lumps[LUMP_FOGS].fileofs);
	s_world->fogs.resize(header->lumps[LUMP_FOGS].filelen / sizeof(*fileFogs));
	auto fileBrushes = (const dbrush_t *)(fileData + header->lumps[LUMP_BRUSHES].fileofs);
	const size_t nBrushes = header->lumps[LUMP_BRUSHES].filelen / sizeof(*fileBrushes);
	auto fileBrushSides = (const dbrushside_t *)(fileData + header->lumps[LUMP_BRUSHSIDES].fileofs);
	const size_t nBrushSides = header->lumps[LUMP_BRUSHSIDES].filelen / sizeof(*fileBrushSides);

	for (size_t i = 0; i < s_world->fogs.size(); i++)
	{
		Fog &f = s_world->fogs[i];
		const dfog_t &ff = fileFogs[i];
		f.originalBrushNumber = LittleLong(ff.brushNum);

		if ((unsigned)f.originalBrushNumber >= nBrushes)
		{
			interface::Error("fog brushNumber out of range");
		}

		const dbrush_t &brush = fileBrushes[f.originalBrushNumber];
		const int firstSide = LittleLong(brush.firstSide);

		if ((unsigned)firstSide > nBrushSides - 6)
		{
			interface::Error("fog brush side number out of range");
		}

		// Brushes are always sorted with the axial sides first.
		int sideNum = firstSide + 0;
		int planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[0][0] = -s_world->planes[planeNum].distance;

		sideNum = firstSide + 1;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[1][0] = s_world->planes[planeNum].distance;

		sideNum = firstSide + 2;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[0][1] = -s_world->planes[planeNum].distance;

		sideNum = firstSide + 3;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[1][1] = s_world->planes[planeNum].distance;

		sideNum = firstSide + 4;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[0][2] = -s_world->planes[planeNum].distance;

		sideNum = firstSide + 5;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[1][2] = s_world->planes[planeNum].distance;

		// Get information from the material for fog parameters.
		Material *material = g_materialCache->findMaterial(ff.shader, MaterialLightmapId::None, true);
		f.parms = material->fogParms;
		((uint8_t *)&f.colorInt)[0] = uint8_t(f.parms.color[0] * g_identityLight * 255);
		((uint8_t *)&f.colorInt)[1] = uint8_t(f.parms.color[1] * g_identityLight * 255);
		((uint8_t *)&f.colorInt)[2] = uint8_t(f.parms.color[2] * g_identityLight * 255);
		((uint8_t *)&f.colorInt)[3] = 255;
		const float d = f.parms.depthForOpaque < 1 ? 1 : f.parms.depthForOpaque;
		f.tcScale = 1.0f / (d * 8);

		// Set the gradient vector.
		sideNum = LittleLong(ff.visibleSide);
		f.hasSurface = sideNum != -1;

		if (f.hasSurface)
		{
			planeNum = LittleLong(fileBrushSides[firstSide + sideNum].planeNum);
			f.surface = vec4(-vec3(s_world->planes[planeNum].normal), -s_world->planes[planeNum].distance);
		}
	}

	// Lightmaps
	if (header->lumps[LUMP_LIGHTMAPS].filelen > 0)
	{
		const size_t srcDataSize = s_world->lightmapSize * s_world->lightmapSize * 3;
		const uint8_t *srcData = &fileData[header->lumps[LUMP_LIGHTMAPS].fileofs];
		const size_t nLightmaps = header->lumps[LUMP_LIGHTMAPS].filelen / srcDataSize;

		if (nLightmaps)
		{
			// Figure out how atlas dimensions by packing lightmaps into cells.
			const int maxCells = 4; // 4x128 = 512

			if (nLightmaps <= maxCells)
			{
				// Simple case.
				s_world->lightmapAtlasSize.x = (int)nLightmaps;
				s_world->lightmapAtlasSize.y = 1;
			}
			else
			{
				if ((nLightmaps & (nLightmaps - 1)) == 0)
				{
					// Power of two. Use square size.
					s_world->lightmapAtlasSize.x = s_world->lightmapAtlasSize.y = (int)ceil(sqrtf((float)nLightmaps));
				}
				else
				{
					s_world->lightmapAtlasSize.x = std::min(maxCells, (int)nLightmaps);
					s_world->lightmapAtlasSize.y = (int)ceil(nLightmaps / (float)s_world->lightmapAtlasSize.x);
				}
			}

			s_world->lightmapAtlasSize.x = std::min(s_world->lightmapAtlasSize.x, maxCells);
			s_world->lightmapAtlasSize.y = std::min(s_world->lightmapAtlasSize.y, maxCells);
			s_world->nLightmapsPerAtlas = s_world->lightmapAtlasSize.x * s_world->lightmapAtlasSize.y;
			s_world->lightmapAtlases.resize((size_t)ceil(nLightmaps / (float)s_world->nLightmapsPerAtlas));

			// Pack lightmaps into atlas(es).
			interface::Printf("Packing %d lightmaps into %d atlas(es) sized %dx%d.\n", (int)nLightmaps, (int)s_world->lightmapAtlases.size(), s_world->lightmapAtlasSize.x * s_world->lightmapSize, s_world->lightmapAtlasSize.y * s_world->lightmapSize);
			size_t lightmapIndex = 0;

			for (size_t i = 0; i < s_world->lightmapAtlases.size(); i++)
			{
				Image image;
				image.width = s_world->lightmapAtlasSize.x * s_world->lightmapSize;
				image.height = s_world->lightmapAtlasSize.y * s_world->lightmapSize;
				image.nComponents = 4;
				image.dataSize = image.width * image.height * image.nComponents;
				image.data = (uint8_t *)malloc(image.dataSize);
				image.release = ReleaseLightmapAtlasImage;
				int nAtlasedLightmaps = 0;

				for (;;)
				{
					// Expand from 24bpp to 32bpp with overbright and RGBM encoding.
					for (int y = 0; y < s_world->lightmapSize; y++)
					{
						for (int x = 0; x < s_world->lightmapSize; x++)
						{
							const uint8_t *src = &srcData[(x + y * s_world->lightmapSize) * 3];
							const int lightmapX = (nAtlasedLightmaps % s_world->nLightmapsPerAtlas) % s_world->lightmapAtlasSize.x;
							const int lightmapY = (nAtlasedLightmaps % s_world->nLightmapsPerAtlas) / s_world->lightmapAtlasSize.x;
							auto dest = (vec4b *)&image.data[((lightmapX * s_world->lightmapSize + x) + (lightmapY * s_world->lightmapSize + y) * (s_world->lightmapAtlasSize.x * s_world->lightmapSize)) * image.nComponents];
							*dest = vec4b(vec4(util::OverbrightenColor(vec3::fromBytes(src)), 1));
						}
					}

					nAtlasedLightmaps++;
					lightmapIndex++;
					srcData += srcDataSize;

					if (nAtlasedLightmaps >= s_world->nLightmapsPerAtlas || lightmapIndex >= nLightmaps)
						break;
				}

				s_world->lightmapAtlases[i] = g_textureCache->create(util::VarArgs("*lightmap%d", (int)i), image, TextureFlags::ClampToEdge | TextureFlags::Mutable);
			}
		}
	}

	// Models
	auto fileModels = (const dmodel_t *)(fileData + header->lumps[LUMP_MODELS].fileofs);
	s_world->modelDefs.resize(header->lumps[LUMP_MODELS].filelen / sizeof(*fileModels));

	for (size_t i = 0; i < s_world->modelDefs.size(); i++)
	{
		ModelDef &m = s_world->modelDefs[i];
		const dmodel_t &fm = fileModels[i];
		m.firstSurface = LittleLong(fm.firstSurface);
		m.nSurfaces = LittleLong(fm.numSurfaces);
		m.bounds[0] = vec3(LittleLong(fm.mins[0]), LittleLong(fm.mins[1]), LittleLong(fm.mins[2]));
		m.bounds[1] = vec3(LittleLong(fm.maxs[0]), LittleLong(fm.maxs[1]), LittleLong(fm.maxs[2]));
	}

	// Light grid. Models must be parsed first.
	{
		assert(s_world->modelDefs.size() > 0);
		lump_t &lump = header->lumps[LUMP_LIGHTGRID];

		s_world->lightGridInverseSize.x = 1.0f / s_world->lightGridSize.x;
		s_world->lightGridInverseSize.y = 1.0f / s_world->lightGridSize.y;
		s_world->lightGridInverseSize.z = 1.0f / s_world->lightGridSize.z;

		for (size_t i = 0; i < 3; i++)
		{
			s_world->lightGridOrigin[i] = s_world->lightGridSize[i] * ceil(s_world->modelDefs[0].bounds.min[i] / s_world->lightGridSize[i]);
			const float max = s_world->lightGridSize[i] * floor(s_world->modelDefs[0].bounds.max[i] / s_world->lightGridSize[i]);
			s_world->lightGridBounds[i] = int((max - s_world->lightGridOrigin[i]) / s_world->lightGridSize[i] + 1);
		}

		const int numGridPoints = s_world->lightGridBounds[0] * s_world->lightGridBounds[1] * s_world->lightGridBounds[2];

		if (lump.filelen != numGridPoints * 8)
		{
			interface::PrintWarningf("WARNING: light grid mismatch\n");
		}
		else
		{
			s_world->lightGridData.resize(lump.filelen);
			memcpy(s_world->lightGridData.data(), &fileData[lump.fileofs], lump.filelen);
		}

		// deal with overbright bits
		for (int i = 0; i < numGridPoints; i++)
		{
			util::OverbrightenColor(&s_world->lightGridData[i*8], &s_world->lightGridData[i*8]);
			util::OverbrightenColor(&s_world->lightGridData[i*8+3], &s_world->lightGridData[i*8+3]);
		}
	}

	// Materials
	auto fileMaterial = (const dshader_t *)(fileData + header->lumps[LUMP_SHADERS].fileofs);
	s_world->materials.resize(header->lumps[LUMP_SHADERS].filelen / sizeof(*fileMaterial));

	for (MaterialDef &m : s_world->materials)
	{
		util::Strncpyz(m.name, fileMaterial->shader, sizeof(m.name));
		m.surfaceFlags = LittleLong(fileMaterial->surfaceFlags);
		m.contentFlags = LittleLong(fileMaterial->contentFlags);
		fileMaterial++;
	}

	// Vertices
	std::vector<Vertex> vertices(header->lumps[LUMP_DRAWVERTS].filelen / sizeof(drawVert_t));
	auto fileDrawVerts = (const drawVert_t *)(fileData + header->lumps[LUMP_DRAWVERTS].fileofs);

	for (size_t i = 0; i < vertices.size(); i++)
	{
		Vertex &v = vertices[i];
		const drawVert_t &fv = fileDrawVerts[i];
		v.pos = vec3(LittleFloat(fv.xyz[0]), LittleFloat(fv.xyz[1]), LittleFloat(fv.xyz[2]));
		v.setNormal(LittleFloat(fv.normal[0]), LittleFloat(fv.normal[1]), LittleFloat(fv.normal[2]));
		v.setTexCoord(LittleFloat(fv.st[0]), LittleFloat(fv.st[1]), LittleFloat(fv.lightmap[0]), LittleFloat(fv.lightmap[1]));
		v.setColor(util::ToLinear(vec4(util::OverbrightenColor(vec3::fromBytes(fv.color)), fv.color[3] / 255.0f)));
	}

	// Indices
	std::vector<uint16_t> indices(header->lumps[LUMP_DRAWINDEXES].filelen / sizeof(int));
	auto fileDrawIndices = (const int *)(fileData + header->lumps[LUMP_DRAWINDEXES].fileofs);

	for (size_t i = 0; i < indices.size(); i++)
	{
		indices[i] = LittleLong(fileDrawIndices[i]);
	}

	// Surfaces
	s_world->surfaces.resize(header->lumps[LUMP_SURFACES].filelen / sizeof(dsurface_t));
	auto fileSurfaces = (const dsurface_t *)(fileData + header->lumps[LUMP_SURFACES].fileofs);

	for (size_t i = 0; i < s_world->surfaces.size(); i++)
	{
		Surface &s = s_world->surfaces[i];
		const dsurface_t &fs = fileSurfaces[i];
		s.fogIndex = LittleLong(fs.fogNum); // -1 means no fog
		const int type = LittleLong(fs.surfaceType);
		int lightmapIndex = LittleLong(fs.lightmapNum);

		// Trisoup is always vertex lit.
		if (type == MST_TRIANGLE_SOUP)
		{
			lightmapIndex = MaterialLightmapId::Vertex;
		}

		const int shaderNum = LittleLong(fs.shaderNum);
		s.material = FindMaterial(shaderNum, lightmapIndex);
		s.flags = s_world->materials[shaderNum].surfaceFlags;
		s.contentFlags = s_world->materials[shaderNum].contentFlags;

		// We may have a nodraw surface, because they might still need to be around for movement clipping.
		if (s.material->surfaceFlags & SURF_NODRAW || s_world->materials[shaderNum].surfaceFlags & SURF_NODRAW)
		{
			s.type = SurfaceType::Ignore;
		}
		else if (type == MST_PLANAR)
		{
			s.type = SurfaceType::Face;
			const int firstVertex = LittleLong(fs.firstVert);
			const int nVertices = LittleLong(fs.numVerts);
			SetSurfaceGeometry(&s, &vertices[firstVertex], nVertices, &indices[LittleLong(fs.firstIndex)], LittleLong(fs.numIndexes), lightmapIndex);

			// Setup cullinfo.
			s.cullinfo.type = CullInfoType::Box | CullInfoType::Plane;
			s.cullinfo.bounds.setupForAddingPoints();

			for (int i = 0; i < nVertices; i++)
			{
				s.cullinfo.bounds.addPoint(vertices[firstVertex + i].pos);
			}

			// take the plane information from the lightmap vector
			for (int i = 0; i < 3; i++)
			{
				s.cullinfo.plane.normal[i] = LittleFloat(fs.lightmapVecs[2][i]);
			}

			s.cullinfo.plane.distance = vec3::dotProduct(vertices[firstVertex].pos, s.cullinfo.plane.normal);
			s.cullinfo.plane.setupFastBoundsTest();
		}
		else if (type == MST_TRIANGLE_SOUP)
		{
			s.type = SurfaceType::Mesh;
			const int firstVertex = LittleLong(fs.firstVert);
			const int nVertices = LittleLong(fs.numVerts);
			SetSurfaceGeometry(&s, &vertices[firstVertex], nVertices, &indices[LittleLong(fs.firstIndex)], LittleLong(fs.numIndexes), lightmapIndex);

			// Setup cullinfo.
			s.cullinfo.bounds.setupForAddingPoints();

			for (int i = 0; i < nVertices; i++)
			{
				s.cullinfo.bounds.addPoint(vertices[firstVertex + i].pos);
			}
		}
		else if (type == MST_PATCH)
		{
			s.type = SurfaceType::Patch;
			s.patch = Patch_Subdivide(LittleLong(fs.patchWidth), LittleLong(fs.patchHeight), &vertices[LittleLong(fs.firstVert)]);
			SetSurfaceGeometry(&s, s.patch->verts, s.patch->numVerts, s.patch->indexes, s.patch->numIndexes, lightmapIndex);
		}
		else if (type == MST_FLARE)
		{
			s.type = SurfaceType::Flare;
		}
	}

	// Create brush models.
	for (size_t i = 1; i < s_world->modelDefs.size(); i++)
	{
		auto model = std::make_unique<WorldModel>((int)i);
		model->buildGeometry();
		g_modelCache->addModel(std::move(model));
	}

	// Leaf surfaces
	auto fileLeafSurfaces = (const int *)(fileData + header->lumps[LUMP_LEAFSURFACES].fileofs);
	s_world->leafSurfaces.resize(header->lumps[LUMP_LEAFSURFACES].filelen / sizeof(int));

	for (size_t i = 0; i < s_world->leafSurfaces.size(); i++)
	{
		s_world->leafSurfaces[i] = LittleLong(fileLeafSurfaces[i]);
	}

	// Nodes and leaves
	auto fileNodes = (const dnode_t *)(fileData + header->lumps[LUMP_NODES].fileofs);
	auto fileLeaves = (const dleaf_t *)(fileData + header->lumps[LUMP_LEAFS].fileofs);
	const size_t nNodes = header->lumps[LUMP_NODES].filelen / sizeof(dnode_t);
	const size_t nLeaves = header->lumps[LUMP_LEAFS].filelen / sizeof(dleaf_t);
	s_world->nodes.resize(nNodes + nLeaves);

	for (size_t i = 0; i < nNodes; i++)
	{
		Node &n = s_world->nodes[i];
		const dnode_t &fn = fileNodes[i];

		n.leaf = false;
		n.bounds[0] = vec3((float)LittleLong(fn.mins[0]), (float)LittleLong(fn.mins[1]), (float)LittleLong(fn.mins[2]));
		n.bounds[1] = vec3((float)LittleLong(fn.maxs[0]), (float)LittleLong(fn.maxs[1]), (float)LittleLong(fn.maxs[2]));
		n.plane = &s_world->planes[LittleLong(fn.planeNum)];

		for (size_t j = 0; j < 2; j++)
		{
			const int c = LittleLong(fn.children[j]);
			n.children[j] = &s_world->nodes[c >= 0 ? c : nNodes + (-1 - c)];
		}
	}

	s_world->firstLeaf = nNodes;

	for (size_t i = 0; i < nLeaves; i++)
	{
		Node &l = s_world->nodes[s_world->firstLeaf + i];
		const dleaf_t &fl = fileLeaves[i];

		l.leaf = true;
		l.bounds[0] = vec3((float)LittleLong(fl.mins[0]), (float)LittleLong(fl.mins[1]), (float)LittleLong(fl.mins[2]));
		l.bounds[1] = vec3((float)LittleLong(fl.maxs[0]), (float)LittleLong(fl.maxs[1]), (float)LittleLong(fl.maxs[2]));
		l.cluster = LittleLong(fl.cluster);
		l.area = LittleLong(fl.area);

		if (l.cluster >= s_world->nClusters)
		{
			s_world->nClusters = l.cluster + 1;
		}

		l.firstSurface = LittleLong(fl.firstLeafSurface);
		l.nSurfaces = LittleLong(fl.numLeafSurfaces);
	}

	// Visibility
	const lump_t &visLump = header->lumps[LUMP_VISIBILITY];

	if (visLump.filelen)
	{
		s_world->nClusters = *((int *)(fileData + visLump.fileofs));
		s_world->clusterBytes = *((int *)(fileData + visLump.fileofs + sizeof(int)));

		// CM_Load should have given us the vis data to share, so we don't need to allocate another copy.
		if (g_externalVisData)
		{
			s_world->visData = g_externalVisData;
		}
		else
		{
			s_world->internalVisData.resize(visLump.filelen - sizeof(int) * 2);
			memcpy(&s_world->internalVisData[0], fileData + visLump.fileofs + sizeof(int) * 2, s_world->internalVisData.size());
			s_world->visData = &s_world->internalVisData[0];
		}
	}

	// Initialize geometry buffers.
	// Index buffer is initialized on first use, not here.
	for (size_t i = 0; i < s_world->currentGeometryBuffer + 1; i++)
	{
		s_world->vertexBuffers[i].handle = bgfx::createVertexBuffer(bgfx::makeRef(&s_world->vertices[i][0], uint32_t(s_world->vertices[i].size() * sizeof(Vertex))), Vertex::decl);
	}

	// Create batched surfaces for frustum culling.
	std::vector<Surface *> sortedSurfaces;
	sortedSurfaces.reserve(s_world->modelDefs[0].nSurfaces); // Reserve maximum possible size. Actual size will probably be less due to ignored surfaces.

	for (Surface &surface : s_world->surfaces)
	{
		if (IgnoreSurface(surface) || surface.material->isPortal) // Ignore portals too.
			continue;

		if (surface.material->isSky)
		{
			CreateOrAppendSkySurface(s_world->skySurfaces, surface);
		}
		else
		{
			sortedSurfaces.push_back(&surface);
		}
	}

	std::sort(sortedSurfaces.begin(), sortedSurfaces.end(), SurfaceCompare);
	std::vector<uint16_t> batchedIndices[s_maxWorldGeometryBuffers];
	CreateBatchedSurfaces(sortedSurfaces, &s_world->batchedSurfaces, batchedIndices, &s_world->cpuDeformVertices, &s_world->cpuDeformIndices);

	for (size_t i = 0; i < s_world->currentGeometryBuffer + 1; i++)
	{
		if (batchedIndices[i].empty())
			continue;

		const bgfx::Memory *mem = bgfx::copy(batchedIndices[i].data(), uint32_t(batchedIndices[i].size() * sizeof(uint16_t)));
		s_world->indexBuffers[i].handle = bgfx::createIndexBuffer(mem);
	}
}

void Unload()
{
	s_world.reset(nullptr);
}

bool IsLoaded()
{
	return s_world.get() != nullptr;
}

/*
=============
R_ChopPolyBehindPlane

Out must have space for two more vertexes than in
=============
*/
static void R_ChopPolyBehindPlane(int numInPoints, const vec3 *inPoints, int *numOutPoints, vec3 *outPoints, vec3 normal, float dist, float epsilon)
{
	const int SIDE_FRONT = 0;
	const int SIDE_BACK = 1;
	const int SIDE_ON = 2;
	float		dists[MAX_VERTS_ON_POLY+4] = { 0 };
	int			sides[MAX_VERTS_ON_POLY+4] = { 0 };
	int			counts[3];
	float		dot;
	int			i, j;
	float		d;

	// don't clip if it might overflow
	if (numInPoints >= MAX_VERTS_ON_POLY - 2)
	{
		*numOutPoints = 0;
		return;
	}

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i = 0; i < numInPoints; i++)
	{
		dot = vec3::dotProduct(inPoints[i], normal);
		dot -= dist;
		dists[i] = dot;

		if (dot > epsilon)
		{
			sides[i] = SIDE_FRONT;
		}
		else if (dot < -epsilon)
		{
			sides[i] = SIDE_BACK;
		} 
		else
		{
			sides[i] = SIDE_ON;
		}

		counts[sides[i]]++;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];
	*numOutPoints = 0;

	if (!counts[0])
		return;

	if (!counts[1])
	{
		*numOutPoints = numInPoints;
		memcpy(outPoints, inPoints, numInPoints * sizeof(vec3));
		return;
	}

	for (i = 0; i < numInPoints; i++)
	{
		vec3 p1 = inPoints[i];
		vec3 *clip = &outPoints[*numOutPoints];
		
		if (sides[i] == SIDE_ON)
		{
			*clip = p1;
			(*numOutPoints)++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			*clip = p1;
			(*numOutPoints)++;
			clip = &outPoints[*numOutPoints];
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
		// generate a split point
		vec3 p2 = inPoints[ (i+1) % numInPoints ];
		d = dists[i] - dists[i+1];

		if (d == 0)
		{
			dot = 0;
		}
		else
		{
			dot = dists[i] / d;
		}

		// clip xyz
		for (j=0; j<3; j++)
		{
			(*clip)[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		(*numOutPoints)++;
	}
}

vec2i GetLightmapSize()
{
	return vec2i(s_world->lightmapAtlasSize.x * s_world->lightmapSize, s_world->lightmapAtlasSize.y * s_world->lightmapSize);
}

int GetNumLightmaps()
{
	return (int)s_world->lightmapAtlases.size();
}

Texture *GetLightmap(int index)
{
	return (index >= 0 && index < (int)s_world->lightmapAtlases.size()) ? s_world->lightmapAtlases[index] : nullptr;
}

int GetNumModels()
{
	return (int)s_world->modelDefs.size();
}

int GetNumSurfaces(int modelIndex)
{
	return (int)s_world->modelDefs[modelIndex].nSurfaces;
}

const Surface &GetSurface(int modelIndex, int surfaceIndex)
{
	// surfaceIndex arg is relative to the models' first surface
	return s_world->surfaces[s_world->modelDefs[modelIndex].firstSurface + surfaceIndex];
}

int GetNumVertexBuffers()
{
	return (int)s_world->currentGeometryBuffer + 1;
}

const std::vector<Vertex> &GetVertexBuffer(int index)
{
	return s_world->vertices[index];
}

bool GetEntityToken(char *buffer, int size)
{
	const char *s = util::Parse(&s_world->entityParsePoint, true);
	util::Strncpyz(buffer, s, size);

	if (!s_world->entityParsePoint && !s[0])
	{
		s_world->entityParsePoint = s_world->entityString.data();
		return false;
	}

	return true;
}

bool HasLightGrid()
{
	return !s_world->lightGridData.empty();
}

void SampleLightGrid(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir)
{
	assert(ambientLight);
	assert(directedLight);
	assert(lightDir);
	assert(HasLightGrid()); // false with -nolight maps

	vec3 lightPosition = position - s_world->lightGridOrigin;
	int pos[3];
	float frac[3];

	for (size_t i = 0; i < 3; i++)
	{
		float v = lightPosition[i] * s_world->lightGridInverseSize[i];
		pos[i] = (int)floor(v),
		frac[i] = v - pos[i];
		pos[i] = math::Clamped(pos[i], 0, s_world->lightGridBounds[i] - 1);
	}

	*ambientLight = vec3::empty;
	*directedLight = vec3::empty;
	vec3 direction;

	// Trilerp the light value.
	int gridStep[3];
	gridStep[0] = 8;
	gridStep[1] = 8 * s_world->lightGridBounds[0];
	gridStep[2] = 8 * s_world->lightGridBounds[0] * s_world->lightGridBounds[1];
	const uint8_t *gridData = &s_world->lightGridData[pos[0] * gridStep[0] + pos[1] * gridStep[1] + pos[2] * gridStep[2]];

	float totalFactor = 0;

	for (int i = 0; i < 8; i++)
	{
		float factor = 1.0;
		const uint8_t *data = gridData;
		int j;

		for (j = 0; j < 3; j++)
		{
			if (i & (1<<j))
			{
				// Ignore values outside lightgrid.
				if (pos[j] + 1 > s_world->lightGridBounds[j] - 1)
					break;

				factor *= frac[j];
				data += gridStep[j];
			}
			else
			{
				factor *= 1.0f - frac[j];
			}
		}

		if (j != 3)
			continue;

		// Ignore samples in walls.
		if (!(data[0] + data[1] + data[2] + data[3] + data[4] + data[5]))
			continue;

		totalFactor += factor;

		(*ambientLight)[0] += factor * data[0];
		(*ambientLight)[1] += factor * data[1];
		(*ambientLight)[2] += factor * data[2];
		(*directedLight)[0] += factor * data[3];
		(*directedLight)[1] += factor * data[4];
		(*directedLight)[2] += factor * data[5];

		int lat = data[7];
		int lng = data[6];
		lat *= (g_funcTableSize / 256);
		lng *= (g_funcTableSize / 256);

		// decode X as cos(lat) * sin(long)
		// decode Y as sin(lat) * sin(long)
		// decode Z as cos(long)
		vec3 normal;
		normal[0] = g_sinTable[(lat + (g_funcTableSize / 4)) & g_funcTableMask] * g_sinTable[lng];
		normal[1] = g_sinTable[lat] * g_sinTable[lng];
		normal[2] = g_sinTable[(lng + (g_funcTableSize / 4)) & g_funcTableMask];
		direction += normal * factor;
	}

	if (totalFactor > 0 && totalFactor < 0.99)
	{
		totalFactor = 1.0f / totalFactor;
		(*ambientLight) *= totalFactor;
		(*directedLight) *= totalFactor;
	}

	(*lightDir) = direction;
	lightDir->normalizeFast();
}

Node *LeafFromPosition(vec3 pos)
{
	Node *node = &s_world->nodes[0];

	for (;;)
	{
		if (node->leaf)
			break;

		const float d = vec3::dotProduct(pos, node->plane->normal) - node->plane->distance;
		node = d > 0 ? node->children[0] : node->children[1];
	}

	return node;
}

bool InPvs(vec3 position)
{
	return LeafFromPosition(position)->cluster != -1;
}

bool InPvs(vec3 position1, vec3 position2)
{
	Node *leaf = LeafFromPosition(position1);
	const uint8_t *vis = interface::CM_ClusterPVS(leaf->cluster);
	leaf = LeafFromPosition(position2);
	return ((vis[leaf->cluster >> 3] & (1 << (leaf->cluster & 7))) != 0);
}

int FindFogIndex(vec3 position, float radius)
{
	for (int i = 0; i < (int)s_world->fogs.size(); i++)
	{
		const Fog &fog = s_world->fogs[i];
		int j;

		for (j = 0; j < 3; j++)
		{
			if (position[j] - radius >= fog.bounds.max[j] || position[j] + radius <= fog.bounds.min[j])
				break;
		}

		if (j == 3)
			return i;
	}

	return -1;
}

int FindFogIndex(const Bounds &bounds)
{
	for (int i = 0; i < (int)s_world->fogs.size(); i++)
	{
		if (Bounds::intersect(bounds, s_world->fogs[i].bounds))
			return i;
	}

	return -1;
}

void CalculateFog(int fogIndex, const mat4 &modelMatrix, const mat4 &modelViewMatrix, vec3 cameraPosition, vec3 localViewPosition, const mat3 &cameraRotation, vec4 *fogColor, vec4 *fogDistance, vec4 *fogDepth, float *eyeT)
{
	assert(fogIndex != -1);
	assert(fogDistance);
	assert(fogDepth);
	assert(eyeT);
	const Fog &fog = s_world->fogs[fogIndex];

	if (fogColor)
	{
		(*fogColor)[0] = ((unsigned char *)(&fog.colorInt))[0] / 255.0f;
		(*fogColor)[1] = ((unsigned char *)(&fog.colorInt))[1] / 255.0f;
		(*fogColor)[2] = ((unsigned char *)(&fog.colorInt))[2] / 255.0f;
		(*fogColor)[3] = ((unsigned char *)(&fog.colorInt))[3] / 255.0f;
	}

	// Grab the entity position and rotation from the model matrix instead of passing them in as more parameters.
	const vec3 position(modelMatrix[12], modelMatrix[13], modelMatrix[14]);
	const mat3 rotation(modelMatrix);
	vec3 local = position - cameraPosition;
	(*fogDistance)[0] = -modelViewMatrix[2];
	(*fogDistance)[1] = -modelViewMatrix[6];
	(*fogDistance)[2] = -modelViewMatrix[10];
	(*fogDistance)[3] = vec3::dotProduct(local, cameraRotation[0]);

	// scale the fog vectors based on the fog's thickness
	(*fogDistance) *= fog.tcScale;

	// rotate the gradient vector for this orientation
	if (fog.hasSurface)
	{
		(*fogDepth)[0] = fog.surface[0] * rotation[0][0] + fog.surface[1] * rotation[0][1] + fog.surface[2] * rotation[0][2];
		(*fogDepth)[1] = fog.surface[0] * rotation[1][0] + fog.surface[1] * rotation[1][1] + fog.surface[2] * rotation[1][2];
		(*fogDepth)[2] = fog.surface[0] * rotation[2][0] + fog.surface[1] * rotation[2][1] + fog.surface[2] * rotation[2][2];
		(*fogDepth)[3] = -fog.surface[3] + vec3::dotProduct(position, fog.surface.xyz());
		*eyeT = vec3::dotProduct(localViewPosition, fogDepth->xyz()) + (*fogDepth)[3];
	}
	else
	{
		*eyeT = 1; // non-surface fog always has eye inside
	}
}

static void BoxSurfaces_recursive(Node *node, Bounds bounds, Surface **list, int listsize, int *listlength, vec3 dir)
{
	// do the tail recursion in a loop
	while (!node->leaf)
	{
		int s = node->plane->testBounds(bounds);

		if (s == 1)
		{
			node = node->children[0];
		}
		else if (s == 2)
		{
			node = node->children[1];
		}
		else
		{
			BoxSurfaces_recursive(node->children[0], bounds, list, listsize, listlength, dir);
			node = node->children[1];
		}
	}

	// add the individual surfaces
	for (int i = 0; i < node->nSurfaces; i++)
	{
		Surface *surface = &s_world->surfaces[s_world->leafSurfaces[node->firstSurface + i]];

		if (*listlength >= listsize)
			break;

		// check if the surface has NOIMPACT or NOMARKS set
		if ((surface->material->surfaceFlags & (SURF_NOIMPACT | SURF_NOMARKS)) || (surface->material->contentFlags & CONTENTS_FOG))
		{
			surface->decalDuplicateId = s_world->decalDuplicateSurfaceId;
		}
		// extra check for surfaces to avoid list overflows
		else if (surface->type == SurfaceType::Face)
		{
			// the face plane should go through the box
			int s = surface->cullinfo.plane.testBounds(bounds);

			if (s == 1 || s == 2)
			{
				surface->decalDuplicateId = s_world->decalDuplicateSurfaceId;
			}
			else if (vec3::dotProduct(surface->cullinfo.plane.normal, dir) > -0.5)
			{
				// don't add faces that make sharp angles with the projection direction
				surface->decalDuplicateId = s_world->decalDuplicateSurfaceId;
			}
		}
		else if (surface->type != SurfaceType::Patch && surface->type != SurfaceType::Mesh)
		{
			surface->decalDuplicateId = s_world->decalDuplicateSurfaceId;
		}

		// check the viewCount because the surface may have already been added if it spans multiple leafs
		if (surface->decalDuplicateId != s_world->decalDuplicateSurfaceId)
		{
			surface->decalDuplicateId = s_world->decalDuplicateSurfaceId;
			list[*listlength] = surface;
			(*listlength)++;
		}
	}
}

static void AddMarkFragments(int numClipPoints, vec3 clipPoints[2][MAX_VERTS_ON_POLY], int numPlanes, const vec3 *normals, const float *dists, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer, int *returnedPoints, int *returnedFragments)
{
	// chop the surface by all the bounding planes of the to be projected polygon
	int pingPong = 0;

	for (int i = 0; i < numPlanes; i++)
	{
		R_ChopPolyBehindPlane(numClipPoints, clipPoints[pingPong], &numClipPoints, clipPoints[!pingPong], normals[i], dists[i], 0.5);
		pingPong ^= 1;

		if (numClipPoints == 0)
			break;
	}

	// completely clipped away?
	if (numClipPoints == 0)
		return;

	// add this fragment to the returned list
	if (numClipPoints + (*returnedPoints) > maxPoints)
		return;	// not enough space for this polygon

	markFragment_t *mf = fragmentBuffer + (*returnedFragments);
	mf->firstPoint = (*returnedPoints);
	mf->numPoints = numClipPoints;
	memcpy(pointBuffer + (*returnedPoints), clipPoints[pingPong], numClipPoints * sizeof(vec3));
	(*returnedPoints) += numClipPoints;
	(*returnedFragments)++;
}

int MarkFragments(int numPoints, const vec3 *points, vec3 projection, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)
{
	const float MARKER_OFFSET = 0; // 1
	int				numsurfaces, numPlanes;
	int				i, j, k, m, n;
	int				returnedFragments;
	int				returnedPoints;
	vec3			normals[MAX_VERTS_ON_POLY+2];
	float			dists[MAX_VERTS_ON_POLY+2];
	vec3			clipPoints[2][MAX_VERTS_ON_POLY];
	int				numClipPoints;

	if (numPoints <= 0)
		return 0;
		
	s_world->decalDuplicateSurfaceId++; // double check prevention
	vec3 projectionDir = projection.normal();

	// Find all the brushes that are to be considered.
	Bounds bounds;
	bounds.setupForAddingPoints();

	for (i = 0; i < numPoints; i++)
	{
		bounds.addPoint(points[i]);
		vec3 temp = points[i] + projection;
		bounds.addPoint(temp);
		// make sure we get all the leafs (also the one(s) in front of the hit surface)
		temp = points[i] + projectionDir * -20;
		bounds.addPoint(temp);
	}

	if (numPoints > MAX_VERTS_ON_POLY)
		numPoints = MAX_VERTS_ON_POLY;

	// create the bounding planes for the to be projected polygon
	for (i = 0; i < numPoints; i++)
	{
		vec3 v1 = points[(i + 1) % numPoints] - points[i];
		vec3 v2 = points[i] + projection;
		v2 = points[i] - v2;
		normals[i] = vec3::crossProduct(v1, v2);
		normals[i].normalizeFast();
		dists[i] = vec3::dotProduct(normals[i], points[i]);
	}

	// add near and far clipping planes for projection
	normals[numPoints] = projectionDir;
	dists[numPoints] = vec3::dotProduct(normals[numPoints], points[0]) - 32;
	normals[numPoints + 1] = projectionDir;
	normals[numPoints + 1].invert();
	dists[numPoints+1] = vec3::dotProduct(normals[numPoints+1], points[0]) - 20;
	numPlanes = numPoints + 2;

	numsurfaces = 0;
	Surface *surfaces[64];
	BoxSurfaces_recursive(&s_world->nodes[0], bounds, surfaces, 64, &numsurfaces, projectionDir);
	returnedPoints = 0;
	returnedFragments = 0;

	for (i = 0; i < numsurfaces; i++)
	{
		Surface *surface = surfaces[i];

		if (surface->type == SurfaceType::Patch)
		{
			for (m = 0; m < surface->patch->height - 1; m++)
			{
				for (n = 0; n < surface->patch->width - 1; n++)
				{
					/*
					We triangulate the grid and chop all triangles within the bounding planes of the to be projected polygon. LOD is not taken into account, not such a big deal though.

					It's probably much nicer to chop the grid itself and deal with this grid as a normal SF_GRID surface so LOD will be applied. However the LOD of that chopped grid must be synced with the LOD of the original curve. One way to do this; the chopped grid shares vertices with the original curve. When LOD is applied to the original curve the unused vertices are flagged. Now the chopped curve should skip the flagged vertices. This still leaves the problems with the vertices at the chopped grid edges.

					To avoid issues when LOD applied to "hollow curves" (like the ones around many jump pads) we now just add a 2 unit offset to the triangle vertices. The offset is added in the vertex normal vector direction so all triangles will still fit together. The 2 unit offset should avoid pretty much all LOD problems.
					*/
					numClipPoints = 3;
					Vertex *dv = surface->patch->verts + m * surface->patch->width + n;
					clipPoints[0][0] = dv[0].pos + dv[0].getNormal() * MARKER_OFFSET;
					clipPoints[0][1] = dv[surface->patch->width].pos + dv[surface->patch->width].getNormal() * MARKER_OFFSET;
					clipPoints[0][2] = dv[1].pos + dv[1].getNormal() * MARKER_OFFSET;

					// check the normal of this triangle
					vec3 v1 = clipPoints[0][0] - clipPoints[0][1];
					vec3 v2 = clipPoints[0][2] - clipPoints[0][1];
					vec3 normal = vec3::crossProduct(v1, v2);
					normal.normalizeFast();

					if (vec3::dotProduct(normal, projectionDir) < -0.1)
					{
						// add the fragments of this triangle
						AddMarkFragments(numClipPoints, clipPoints, numPlanes, normals, dists, maxPoints, pointBuffer, maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments);

						if (returnedFragments == maxFragments)
							return returnedFragments;	// not enough space for more fragments
					}

					clipPoints[0][0] = dv[1].pos + dv[1].getNormal() * MARKER_OFFSET;
					clipPoints[0][1] = dv[surface->patch->width].pos + dv[surface->patch->width].getNormal() * MARKER_OFFSET;
					clipPoints[0][2] = dv[surface->patch->width + 1].pos + dv[surface->patch->width + 1].getNormal() * MARKER_OFFSET;

					// check the normal of this triangle
					v1 = clipPoints[0][0] - clipPoints[0][1];
					v2 = clipPoints[0][2] - clipPoints[0][1];
					normal = vec3::crossProduct(v1, v2);
					normal.normalizeFast();

					if (vec3::dotProduct(normal, projectionDir) < -0.05)
					{
						// add the fragments of this triangle
						AddMarkFragments(numClipPoints, clipPoints, numPlanes, normals, dists, maxPoints, pointBuffer, maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments);

						if (returnedFragments == maxFragments)
							return returnedFragments;	// not enough space for more fragments
					}
				}
			}
		}
		else if (surface->type == SurfaceType::Face)
		{
			// check the normal of this face
			if (vec3::dotProduct(surface->cullinfo.plane.normal, projectionDir) > -0.5)
				continue;

			uint16_t *tri;

			for (k = 0, tri = &surface->indices[0]; k < (int)surface->indices.size(); k += 3, tri += 3)
			{
				for (j = 0; j < 3; j++)
				{
					clipPoints[0][j] = s_world->vertices[surface->bufferIndex][tri[j]].pos + surface->cullinfo.plane.normal * MARKER_OFFSET;
				}

				// add the fragments of this face
				AddMarkFragments(3, clipPoints, numPlanes, normals, dists, maxPoints, pointBuffer, maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments);

				if (returnedFragments == maxFragments)
					return returnedFragments;	// not enough space for more fragments
			}
		}
		else if (surface->type == SurfaceType::Mesh)
		{
			uint16_t *tri;

			for (k = 0, tri = &surface->indices[0]; k < (int)surface->indices.size(); k += 3, tri += 3)
			{
				for (j = 0; j < 3; j++)
				{
					clipPoints[0][j] = s_world->vertices[surface->bufferIndex][tri[j]].pos + s_world->vertices[surface->bufferIndex][tri[j]].getNormal() * MARKER_OFFSET;
				}

				// add the fragments of this face
				AddMarkFragments(3, clipPoints, numPlanes, normals, dists, maxPoints, pointBuffer, maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments);

				if (returnedFragments == maxFragments)
					return returnedFragments;	// not enough space for more fragments
			}
		}
	}

	return returnedFragments;
}

Bounds GetBounds()
{
	return s_world->modelDefs[0].bounds;
}

Bounds GetBounds(VisibilityId visId)
{
	return s_world->visibility[(int)visId].bounds;
}

size_t GetNumSkySurfaces(VisibilityId visId)
{
	const Visibility &vis = s_world->visibility[(int)visId];

	if (vis.method == VisibilityMethod::PVS)
	{
		return vis.skySurfaces.size();
	}
	else
	{
		return s_world->skySurfaces.size();
	}
}

const SkySurface &GetSkySurface(VisibilityId visId, size_t index)
{
	const Visibility &vis = s_world->visibility[(int)visId];

	if (vis.method == VisibilityMethod::PVS)
	{
		return vis.skySurfaces[index];
	}
	else
	{
		return s_world->skySurfaces[index];
	}
}

bool CalculatePortalCamera(VisibilityId visId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, const std::vector<renderer::Entity> &entities, vec3 *pvsPosition, Transform *portalCamera, bool *isMirror, Plane *portalPlane)
{
	assert(pvsPosition);
	assert(portalCamera);
	assert(isMirror);
	assert(portalPlane);
	Visibility &vis = s_world->visibility[(int)visId];

	// Calculate which portal surfaces in the PVS are visible to the camera.
	vis.cameraPortalSurfaces.clear();

	for (Surface *portalSurface : vis.portalSurfaces)
	{
		// Trivially reject.
		if (util::IsGeometryOffscreen(mvp, portalSurface->indices.data(), portalSurface->indices.size(), s_world->vertices[portalSurface->bufferIndex].data()))
			continue;

		// Determine if this surface is backfaced and also determine the distance to the nearest vertex so we can cull based on portal range.
		// Culling based on vertex distance isn't 100% correct (we should be checking for range to the surface), but it's good enough for the types of portals we have in the game right now.
		float shortest;

		if (util::IsGeometryBackfacing(mainCameraPosition, portalSurface->indices.data(), portalSurface->indices.size(), s_world->vertices[portalSurface->bufferIndex].data(), &shortest))
			continue;

		// Calculate surface plane.
		Plane plane;

		if (portalSurface->indices.size() >= 3)
		{
			const vec3 v1(s_world->vertices[portalSurface->bufferIndex][portalSurface->indices[0]].pos);
			const vec3 v2(s_world->vertices[portalSurface->bufferIndex][portalSurface->indices[1]].pos);
			const vec3 v3(s_world->vertices[portalSurface->bufferIndex][portalSurface->indices[2]].pos);
			plane.normal = vec3::crossProduct(v3 - v1, v2 - v1).normal();
			plane.distance = vec3::dotProduct(v1, plane.normal);
		}
		else
		{
			plane.normal[0] = 1;
		}

		// Locate the portal entity closest to this plane.
		// Origin will be the origin of the portal, oldorigin will be the origin of the camera.
		const renderer::Entity *portalEntity = nullptr;

		for (size_t j = 0; j < entities.size(); j++)
		{
			const renderer::Entity &entity = entities[j];

			if (entity.type == EntityType::Portal)
			{
				const float d = vec3::dotProduct(entity.position, plane.normal) - plane.distance;

				if (d > 64 || d < -64)
					continue;

				portalEntity = &entity;
				break;
			}
		}

		// If we didn't locate a portal entity, don't render anything.
		// We don't want to just treat it as a mirror, because without a portal entity the server won't have communicated a proper entity set in the snapshot
		// Unfortunately, with local movement prediction it is easily possible to see a surface before the server has communicated the matching portal surface entity.
		if (!portalEntity)
			continue;

		// Mirrors don't do a fade over distance (although they could).
		bool isPortalMirror = (portalEntity->position == portalEntity->oldPosition);

		if (!isPortalMirror && shortest > portalSurface->material->portalRange * portalSurface->material->portalRange)
			continue;

		// Portal surface is visible to the camera.
		Visibility::Portal portal;
		portal.entity = portalEntity;
		portal.isMirror = isPortalMirror;
		portal.plane = plane;
		portal.surface = portalSurface;
		vis.cameraPortalSurfaces.push_back(portal);
	}

	if (vis.cameraPortalSurfaces.empty())
		return false;

	// All visible portal surfaces are required for writing to the stencil buffer, but we only need the first one to figure out the transform.
	const Visibility::Portal &portal = vis.cameraPortalSurfaces[0];

	// Portal surface is visible.
	// Calculate portal camera transform.
	Transform surfaceTransform, cameraTransform;
	surfaceTransform.rotation[0] = portal.plane.normal;
	surfaceTransform.rotation[1] = surfaceTransform.rotation[0].perpendicular();
	surfaceTransform.rotation[2] = vec3::crossProduct(surfaceTransform.rotation[0], surfaceTransform.rotation[1]);

	// If the entity is just a mirror, don't use as a camera point.
	if (portal.isMirror)
	{
		surfaceTransform.position = portal.plane.normal * portal.plane.distance;
		cameraTransform.position = surfaceTransform.position;
		cameraTransform.rotation[0] = -surfaceTransform.rotation[0];
		cameraTransform.rotation[1] = surfaceTransform.rotation[1];
		cameraTransform.rotation[2] = surfaceTransform.rotation[2];
	}
	else
	{
		// Project the origin onto the surface plane to get an origin point we can rotate around.
		const float d = vec3::dotProduct(portal.entity->position, portal.plane.normal) - portal.plane.distance;
		surfaceTransform.position = portal.entity->position + surfaceTransform.rotation[0] * -d;

		// Now get the camera position and rotation.
		cameraTransform.position = portal.entity->oldPosition;
		cameraTransform.rotation[0] = -vec3(portal.entity->rotation[0]);
		cameraTransform.rotation[1] = -vec3(portal.entity->rotation[1]);
		cameraTransform.rotation[2] = portal.entity->rotation[2];

		// Optionally rotate.
		if (portal.entity->oldFrame || portal.entity->skinNum)
		{
			float d;

			if (portal.entity->oldFrame)
			{
				// If a speed is specified.
				if (portal.entity->frame)
				{
					// Continuous rotate
					d = main::GetFloatTime() * portal.entity->frame;
				}
				else
				{
					// Bobbing rotate, with skinNum being the rotation offset.
					d = portal.entity->skinNum + sin(main::GetFloatTime()) * 4;
				}
			}
			else if (portal.entity->skinNum)
			{
				d = (float)portal.entity->skinNum;
			}

			cameraTransform.rotation[1] = cameraTransform.rotation[1].rotatedAroundDirection(cameraTransform.rotation[0], d);
			cameraTransform.rotation[2] = vec3::crossProduct(cameraTransform.rotation[0], cameraTransform.rotation[1]);
		}
	}

	*pvsPosition = portal.entity->oldPosition; // Get the PVS position from the entity.
	portalCamera->position = util::MirroredPoint(mainCameraPosition, surfaceTransform, cameraTransform);
	portalCamera->rotation[0] = util::MirroredVector(mainCameraRotation[0], surfaceTransform, cameraTransform);
	portalCamera->rotation[1] = util::MirroredVector(mainCameraRotation[1], surfaceTransform, cameraTransform);
	portalCamera->rotation[2] = util::MirroredVector(mainCameraRotation[2], surfaceTransform, cameraTransform);
	*isMirror = portal.isMirror;
	*portalPlane = Plane(-cameraTransform.rotation[0], vec3::dotProduct(cameraTransform.position, -cameraTransform.rotation[0]));
	return true;
}

bool CalculateReflectionCamera(VisibilityId visId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, Transform *camera, Plane *plane)
{
	assert(camera);
	assert(plane);
	Visibility &vis = s_world->visibility[(int)visId];

	// Calculate which reflective surfaces in the PVS are visible to the camera.
	vis.cameraReflectiveSurfaces.clear();

	for (Surface *surface : vis.reflectiveSurfaces)
	{
		// Trivially reject.
		if (util::IsGeometryOffscreen(mvp, surface->indices.data(), surface->indices.size(), s_world->vertices[surface->bufferIndex].data()))
			continue;

		// Determine if this surface is backfaced.
		if (util::IsGeometryBackfacing(mainCameraPosition, surface->indices.data(), surface->indices.size(), s_world->vertices[surface->bufferIndex].data()))
			continue;

		// Reflective surface is visible to the camera.
		Visibility::Reflective reflective;
		reflective.surface = surface;

		if (surface->indices.size() >= 3)
		{
			const vec3 v1(s_world->vertices[surface->bufferIndex][surface->indices[0]].pos);
			const vec3 v2(s_world->vertices[surface->bufferIndex][surface->indices[1]].pos);
			const vec3 v3(s_world->vertices[surface->bufferIndex][surface->indices[2]].pos);
			reflective.plane.normal = vec3::crossProduct(v3 - v1, v2 - v1).normal();
			reflective.plane.distance = vec3::dotProduct(v1, reflective.plane.normal);
		}
		else
		{
			reflective.plane.normal[0] = 1;
		}

		vis.cameraReflectiveSurfaces.push_back(reflective);
	}

	if (vis.cameraReflectiveSurfaces.empty())
		return false;

	// All visible reflective surfaces are required for writing to the stencil buffer, but we only need the first one to figure out the transform.
	const Visibility::Reflective &reflective = vis.cameraReflectiveSurfaces[0];
	Transform surfaceTransform, cameraTransform;
	surfaceTransform.rotation[0] = reflective.plane.normal;
	surfaceTransform.rotation[1] = surfaceTransform.rotation[0].perpendicular();
	surfaceTransform.rotation[2] = vec3::crossProduct(surfaceTransform.rotation[0], surfaceTransform.rotation[1]);
	surfaceTransform.position = reflective.plane.normal * reflective.plane.distance;
	cameraTransform.position = surfaceTransform.position;
	cameraTransform.rotation[0] = -surfaceTransform.rotation[0];
	cameraTransform.rotation[1] = surfaceTransform.rotation[1];
	cameraTransform.rotation[2] = surfaceTransform.rotation[2];
	camera->position = util::MirroredPoint(mainCameraPosition, surfaceTransform, cameraTransform);
	camera->rotation[0] = util::MirroredVector(mainCameraRotation[0], surfaceTransform, cameraTransform);
	camera->rotation[1] = util::MirroredVector(mainCameraRotation[1], surfaceTransform, cameraTransform);
	camera->rotation[2] = util::MirroredVector(mainCameraRotation[2], surfaceTransform, cameraTransform);
	*plane = Plane(-cameraTransform.rotation[0], vec3::dotProduct(cameraTransform.position, -cameraTransform.rotation[0]));
	return true;
}

void RenderPortal(VisibilityId visId, DrawCallList *drawCallList)
{
	assert(drawCallList);
	const Visibility &vis = s_world->visibility[(int)visId];

	for (const Visibility::Portal &portal : vis.cameraPortalSurfaces)
	{
		bgfx::TransientIndexBuffer tib;
		auto nIndices = (const uint32_t)portal.surface->indices.size();

		if (bgfx::getAvailTransientIndexBuffer(nIndices) < nIndices)
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			return;
		}

		bgfx::allocTransientIndexBuffer(&tib, nIndices);
		memcpy(tib.data, portal.surface->indices.data(), nIndices * sizeof(uint16_t));

		DrawCall dc;
		dc.material = portal.surface->material;
		dc.vb.type = DrawCall::BufferType::Static;
		dc.vb.staticHandle = s_world->vertexBuffers[portal.surface->bufferIndex].handle;
		dc.vb.nVertices = (uint32_t)s_world->vertices[portal.surface->bufferIndex].size();
		dc.ib.type = DrawCall::BufferType::Transient;
		dc.ib.transientHandle = tib;
		dc.ib.nIndices = nIndices;
		drawCallList->push_back(dc);
	}
}

void RenderReflective(VisibilityId visId, DrawCallList *drawCallList)
{
	assert(drawCallList);
	const Visibility &vis = s_world->visibility[(int)visId];

	for (const Visibility::Reflective &reflective : vis.cameraReflectiveSurfaces)
	{
		bgfx::TransientIndexBuffer tib;
		auto nIndices = (const uint32_t)reflective.surface->indices.size();

		if (bgfx::getAvailTransientIndexBuffer(nIndices) < nIndices)
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			return;
		}

		bgfx::allocTransientIndexBuffer(&tib, nIndices);
		memcpy(tib.data, reflective.surface->indices.data(), nIndices * sizeof(uint16_t));

		DrawCall dc;
		dc.material = reflective.surface->material->reflectiveFrontSideMaterial;
		assert(dc.material);
		dc.vb.type = DrawCall::BufferType::Static;
		dc.vb.staticHandle = s_world->vertexBuffers[reflective.surface->bufferIndex].handle;
		dc.vb.nVertices = (uint32_t)s_world->vertices[reflective.surface->bufferIndex].size();
		dc.ib.type = DrawCall::BufferType::Transient;
		dc.ib.transientHandle = tib;
		dc.ib.nIndices = nIndices;
		drawCallList->push_back(dc);
	}
}

static void UpdatePvsVisibility(VisibilityId visId, vec3 cameraPosition, const uint8_t *areaMask)
{
	assert(areaMask);
	Visibility &vis = s_world->visibility[(int)visId];
	vis.method = VisibilityMethod::PVS;

	// Get the PVS for the camera leaf cluster.
	Node *cameraLeaf = LeafFromPosition(cameraPosition);

	// Build a list of visible surfaces.
	// Don't need to refresh visible surfaces if the camera cluster or the area bitmask haven't changed.
	if (vis.lastCameraLeaf != nullptr && vis.lastCameraLeaf->cluster == cameraLeaf->cluster && std::equal(areaMask, areaMask + MAX_MAP_AREA_BYTES, vis.lastAreaMask))
		return;

	// Clear data that will be recalculated.
	vis.portalSurfaces.clear();
	vis.reflectiveSurfaces.clear();
	vis.skySurfaces.clear();
	vis.surfaces.clear();
	vis.bounds.setupForAddingPoints();

	// A cluster of -1 means the camera is outside the PVS - draw everything.
	const uint8_t *pvs = cameraLeaf->cluster == -1 ? nullptr: &s_world->visData[cameraLeaf->cluster * s_world->clusterBytes];

	for (size_t i = s_world->firstLeaf; i < s_world->nodes.size(); i++)
	{
		Node &leaf = s_world->nodes[i];

		// Check PVS.
		if (pvs && !(pvs[leaf.cluster >> 3] & (1 << (leaf.cluster & 7))))
			continue;

		// Check for door connection.
		if (pvs && (areaMask[leaf.area >> 3] & (1 << (leaf.area & 7))))
			continue;

		// Merge this leaf's bounds.
		vis.bounds.addPoints(leaf.bounds);

		for (int j = 0; j < leaf.nSurfaces; j++)
		{
			const int si = s_world->leafSurfaces[leaf.firstSurface + j];
			Surface &surface = s_world->surfaces[si];

			// Ignore surfaces in brush models.
			if (si < 0 || si >= (int)s_world->modelDefs[0].nSurfaces)
				continue;

			// Don't add duplicates.
			if (surface.duplicateId == s_world->duplicateSurfaceId)
				continue;

			// Ignore flares.
			if (IgnoreSurface(surface))
				continue;

			// Add the surface.
			surface.duplicateId = s_world->duplicateSurfaceId;
					
			if (surface.material->isSky)
			{
				CreateOrAppendSkySurface(vis.skySurfaces, surface);
			}
			else
			{
				if (surface.material->reflective == MaterialReflective::BackSide)
				{
					vis.reflectiveSurfaces.push_back(&surface);
				}

				if (surface.material->isPortal)
				{
					vis.portalSurfaces.push_back(&surface);
				}

				vis.surfaces.push_back(&surface);
			}
		}
	}

	// Sort visible surfaces.
	std::sort(vis.surfaces.begin(), vis.surfaces.end(), SurfaceCompare);

	CreateBatchedSurfaces(vis.surfaces, &vis.batchedSurfaces, vis.indices, &vis.cpuDeformVertices, &vis.cpuDeformIndices);

	// Update dynamic index buffers.
	for (size_t i = 0; i < s_world->currentGeometryBuffer + 1; i++)
	{
		DynamicIndexBuffer &ib = vis.indexBuffers[i];
		std::vector<uint16_t> &indices = vis.indices[i];

		if (indices.empty())
			continue;

		const bgfx::Memory *mem = bgfx::copy(indices.data(), uint32_t(indices.size() * sizeof(uint16_t)));

		// Buffer is created on first use.
		if (!bgfx::isValid(ib.handle))
		{
			ib.handle = bgfx::createDynamicIndexBuffer(mem, BGFX_BUFFER_ALLOW_RESIZE);
		}
		else				
		{
			bgfx::updateDynamicIndexBuffer(ib.handle, 0, mem);
		}
	}

	s_world->duplicateSurfaceId++;
	vis.lastCameraLeaf = cameraLeaf;
	memcpy(vis.lastAreaMask, areaMask, sizeof(vis.lastAreaMask));
}

static void UpdateCameraFrustumVisibility(VisibilityId visId, vec3 cameraPosition, const uint8_t *areaMask)
{
	Visibility &vis = s_world->visibility[(int)visId];
	vis.method = VisibilityMethod::CameraFrustum;
	vis.bounds.setupForAddingPoints();

	for (const BatchedSurface &bs : s_world->batchedSurfaces)
	{
		vis.bounds.addPoints(bs.bounds);
	}
}

void UpdateVisibility(VisibilityId visId, vec3 cameraPosition, const uint8_t *areaMask)
{
	if (visId == VisibilityId::Probe)
	{
		UpdateCameraFrustumVisibility(visId, cameraPosition, areaMask);
	}
	else
	{
		UpdatePvsVisibility(visId, cameraPosition, areaMask);
	}
}

void Render(VisibilityId visId, DrawCallList *drawCallList, const mat3 &sceneRotation)
{
	assert(drawCallList);
	const Visibility &vis = s_world->visibility[(int)visId];
	const std::vector<BatchedSurface> *batchedSurfaces;
	const std::vector<Vertex> *cpuDeformVertices;
	const std::vector<uint16_t> *cpuDeformIndices;

	if (vis.method == VisibilityMethod::PVS)
	{
		batchedSurfaces = &vis.batchedSurfaces;
		cpuDeformVertices = &vis.cpuDeformVertices;
		cpuDeformIndices = &vis.cpuDeformIndices;
	}
	else
	{
		batchedSurfaces = &s_world->batchedSurfaces;
		cpuDeformVertices = &s_world->cpuDeformVertices;
		cpuDeformIndices = &s_world->cpuDeformIndices;
	}

	for (const BatchedSurface &surface : *batchedSurfaces)
	{
		DrawCall dc;
		dc.flags = 0;

		if (surface.surfaceFlags & SURF_SKY)
			dc.flags |= DrawCallFlags::Sky;

		dc.fogIndex = surface.fogIndex;
		dc.material = surface.material;

		if (main::AreWaterReflectionsEnabled())
		{
			// If this is a back side reflective material, use the front side material if there's any reflective surfaces visible to the camera.
			if (dc.material->reflective == MaterialReflective::BackSide && !vis.cameraReflectiveSurfaces.empty())
			{
				dc.material = dc.material->reflectiveFrontSideMaterial;
			}
		}

		if (surface.material->hasAutoSpriteDeform())
		{
			assert(!cpuDeformVertices->empty() && !cpuDeformIndices->empty());
			assert(surface.nVertices);
			assert(surface.nIndices);

			// Copy the CPU deform geo to a transient buffer.
			bgfx::TransientVertexBuffer tvb;
			bgfx::TransientIndexBuffer tib;

			if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, surface.nVertices, &tib, surface.nIndices))
			{
				WarnOnce(WarnOnceId::TransientBuffer);
				continue;
			}
				
			memcpy(tib.data, &(*cpuDeformIndices)[surface.firstIndex], surface.nIndices * sizeof(uint16_t));
			memcpy(tvb.data, &(*cpuDeformVertices)[surface.firstVertex], surface.nVertices * sizeof(Vertex));
			dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
			dc.vb.transientHandle = tvb;
			dc.vb.nVertices = surface.nVertices;
			dc.ib.transientHandle = tib;
			dc.ib.nIndices = surface.nIndices;

			// Deform the transient buffer contents.
			surface.material->doAutoSpriteDeform(sceneRotation, (Vertex *)tvb.data, surface.nVertices, (uint16_t *)tib.data, surface.nIndices, &dc.softSpriteDepth);
		}
		else
		{
			dc.vb.type = DrawCall::BufferType::Static;
			dc.vb.staticHandle = s_world->vertexBuffers[surface.bufferIndex].handle;
			dc.vb.nVertices = (uint32_t)s_world->vertices[surface.bufferIndex].size();

			if (vis.method == VisibilityMethod::PVS)
			{
				dc.ib.type = DrawCall::BufferType::Dynamic;
				dc.ib.dynamicHandle = vis.indexBuffers[surface.bufferIndex].handle;
			}
			else
			{
				dc.ib.type = DrawCall::BufferType::Static;
				dc.ib.staticHandle = s_world->indexBuffers[surface.bufferIndex].handle;
			}

			dc.ib.firstIndex = surface.firstIndex;
			dc.ib.nIndices = surface.nIndices;
		}

		drawCallList->push_back(dc);
	}
}

void PickMaterial()
{
	const Transform camera = main::GetMainCameraTransform();
	const Surface *closestSurface = nullptr;
	float closestDistance = 10000.0f;

	for (const ModelDef &model : s_world->modelDefs)
	{
		for (size_t si = 0; si < model.nSurfaces; si++)
		{
			const Surface &surface = s_world->surfaces[model.firstSurface + si];

			for (size_t i = 0; i < surface.indices.size(); i += 3)
			{
				const Vertex *verts[3];

				for (size_t vi = 0; vi < 3; vi++)
					verts[vi] = &s_world->vertices[surface.bufferIndex][surface.indices[i + 2 - vi]];

				// Fast Minimum Storage Ray/Triangle Intersection by Moller and Trumbore
				const vec3 edge1 = verts[1]->pos - verts[0]->pos;
				const vec3 edge2 = verts[2]->pos - verts[0]->pos;
				const vec3 pvec = vec3::crossProduct(camera.rotation[0], edge2);
				const float det = vec3::dotProduct(edge1, pvec);

				if (det < 0.000001f)
					continue;

				const vec3 tvec = camera.position - verts[0]->pos;
				const float u = vec3::dotProduct(tvec, pvec);

				if (u < 0 || u > det)
					continue;

				const vec3 qvec = vec3::crossProduct(tvec, edge1);
				const float v = vec3::dotProduct(camera.rotation[0], qvec);

				if (v < 0 || u + v > det)
					continue;

				const float t = vec3::dotProduct(edge2, qvec) * (1.0f / det);

				if (t < 0)
					continue;

				if (t < closestDistance)
				{
					closestDistance = t;
					closestSurface = &surface;
				}
			}
		}
	}

	if (closestSurface)
	{
		interface::Printf("%s\n", closestSurface->material->name);

		for (int i = 0; i < closestSurface->material->numUnfoggedPasses; i++)
		{
			interface::Printf("   %2d : %s\n", i, closestSurface->material->stages[i].bundles[MaterialTextureBundleIndex::DiffuseMap].textures[0]->getName());
		}
	}
	else
	{
		interface::Printf("Picking failed\n");
	}
}

} // namespace world
} // namespace renderer
