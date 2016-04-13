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

/*
==============================================================================

.BSP file format

==============================================================================
*/


#define BSP_IDENT	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
// little-endian "IBSP"

#if defined(ENGINE_IOQ3)
#define BSP_VERSION			46
#elif defined(ENGINE_IORTCW)
#define BSP_VERSION			47
#else
#error Engine undefined
#endif


// there shouldn't be any problem with increasing these values at the
// expense of more memory allocation in the utilities

#if defined(ENGINE_IOQ3)
#define	MAX_MAP_MODELS		0x400
#elif defined(ENGINE_IORTCW)
#define	MAX_MAP_MODELS		0x800
#else
#error Engine undefined
#endif
	
#define	MAX_MAP_BRUSHES		0x8000
#define	MAX_MAP_ENTITIES	0x800
#define	MAX_MAP_ENTSTRING	0x40000
#define	MAX_MAP_SHADERS		0x400

#define	MAX_MAP_AREAS		0x100	// MAX_MAP_AREA_BYTES in q_shared must match!
#define	MAX_MAP_FOGS		0x100
#define	MAX_MAP_PLANES		0x20000
#define	MAX_MAP_NODES		0x20000
#define	MAX_MAP_BRUSHSIDES	0x20000
#define	MAX_MAP_LEAFS		0x20000
#define	MAX_MAP_LEAFFACES	0x20000
#define	MAX_MAP_LEAFBRUSHES 0x40000
#define	MAX_MAP_PORTALS		0x20000
#define	MAX_MAP_LIGHTING	0x800000
#define	MAX_MAP_LIGHTGRID	0x800000
#define	MAX_MAP_VISIBILITY	0x200000

#define	MAX_MAP_DRAW_SURFS	0x20000
#define	MAX_MAP_DRAW_VERTS	0x80000
#define	MAX_MAP_DRAW_INDEXES	0x80000


// key / value pair sizes in the entities lump
#define	MAX_KEY				32
#define	MAX_VALUE			1024

// the editor uses these predefined yaw angles to orient entities up or down
#define	ANGLE_UP			-1
#define	ANGLE_DOWN			-2

#define	LIGHTMAP_WIDTH		128
#define	LIGHTMAP_HEIGHT		128

#define MAX_WORLD_COORD		( 128*1024 )
#define MIN_WORLD_COORD		( -128*1024 )
#define WORLD_SIZE			( MAX_WORLD_COORD - MIN_WORLD_COORD )

//=============================================================================


typedef struct {
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES		0
#define	LUMP_SHADERS		1
#define	LUMP_PLANES			2
#define	LUMP_NODES			3
#define	LUMP_LEAFS			4
#define	LUMP_LEAFSURFACES	5
#define	LUMP_LEAFBRUSHES	6
#define	LUMP_MODELS			7
#define	LUMP_BRUSHES		8
#define	LUMP_BRUSHSIDES		9
#define	LUMP_DRAWVERTS		10
#define	LUMP_DRAWINDEXES	11
#define	LUMP_FOGS			12
#define	LUMP_SURFACES		13
#define	LUMP_LIGHTMAPS		14
#define	LUMP_LIGHTGRID		15
#define	LUMP_VISIBILITY		16
#define	HEADER_LUMPS		17

typedef struct {
	int			ident;
	int			version;

	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
	float		mins[3], maxs[3];
	int			firstSurface, numSurfaces;
	int			firstBrush, numBrushes;
} dmodel_t;

typedef struct {
	char		shader[MAX_QPATH];
	int			surfaceFlags;
	int			contentFlags;
} dshader_t;

// planes x^1 is allways the opposite of plane x

typedef struct {
	float		normal[3];
	float		dist;
} dplane_t;

typedef struct {
	int			planeNum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	int			mins[3];		// for frustom culling
	int			maxs[3];
} dnode_t;

typedef struct {
	int			cluster;			// -1 = opaque cluster (do I still store these?)
	int			area;

	int			mins[3];			// for frustum culling
	int			maxs[3];

	int			firstLeafSurface;
	int			numLeafSurfaces;

	int			firstLeafBrush;
	int			numLeafBrushes;
} dleaf_t;

typedef struct {
	int			planeNum;			// positive plane side faces out of the leaf
	int			shaderNum;
} dbrushside_t;

typedef struct {
	int			firstSide;
	int			numSides;
	int			shaderNum;		// the shader that determines the contents flags
} dbrush_t;

typedef struct {
	char		shader[MAX_QPATH];
	int			brushNum;
	int			visibleSide;	// the brush side that ray tests need to clip against (-1 == none)
} dfog_t;

typedef struct {
	vec3_t		xyz;
	float		st[2];
	float		lightmap[2];
	vec3_t		normal;
	byte		color[4];
} drawVert_t;

#define drawVert_t_cleared(x) drawVert_t (x) = {{0, 0, 0}, {0, 0}, {0, 0}, {0, 0, 0}, {0, 0, 0, 0}}

typedef enum {
	MST_BAD,
	MST_PLANAR,
	MST_PATCH,
	MST_TRIANGLE_SOUP,
	MST_FLARE
} mapSurfaceType_t;

typedef struct {
	int			shaderNum;
	int			fogNum;
	int			surfaceType;

	int			firstVert;
	int			numVerts;

	int			firstIndex;
	int			numIndexes;

	int			lightmapNum;
	int			lightmapX, lightmapY;
	int			lightmapWidth, lightmapHeight;

	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[3];	// for patches, [0] and [1] are lodbounds

	int			patchWidth;
	int			patchHeight;
} dsurface_t;

void World::load(const char *name)
{
	util::Strncpyz(name_, name, sizeof(name_));
	util::Strncpyz(baseName_, util::SkipPath(name_), sizeof(baseName_));
	util::StripExtension(baseName_, baseName_, sizeof(baseName_));

	ReadOnlyFile file(name_);

	if (!file.isValid())
	{
		interface::Error("%s not found", name_);
		return;
	}

	fileData_ = file.getData();

	// Header
	auto header = (dheader_t *)fileData_;

	const int version = LittleLong(header->version);

	if (version != BSP_VERSION)
	{
		interface::Error("%s has wrong version number (%i should be %i)", name_, version, BSP_VERSION);
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
		interface::Error("%s: lump %d has bad size", name_, (int)i);
	}

	// Entities
	{
		lump_t &lump = header->lumps[LUMP_ENTITIES];
		auto p = (char *)(&fileData_[lump.fileofs]);

		// Store for reference by the cgame.
		entityString_.resize(lump.filelen + 1);
		strcpy(entityString_.data(), p);
		entityParsePoint_ = entityString_.data();

		char *token = util::Parse(&p, true);

		if (*token && *token == '{')
		{
			for (;;)
			{
				// Parse key.
				token = util::Parse(&p, true);

				if (!*token || *token == '}')
					break;

				char keyname[MAX_TOKEN_CHARS];
				util::Strncpyz(keyname, token, sizeof(keyname));

				// Parse value.
				token = util::Parse(&p, true);

				if (!*token || *token == '}')
					break;

				char value[MAX_TOKEN_CHARS];
				util::Strncpyz(value, token, sizeof(value));

				// Check for a different light grid size.
				if (!util::Stricmp(keyname, "gridsize"))
				{
					sscanf(value, "%f %f %f", &lightGridSize_.x, &lightGridSize_.y, &lightGridSize_.z);
					continue;
				}
			}
		}
	}

	// Planes
	// Needs to be loaded before fogs.
	auto filePlane = (const dplane_t *)(fileData_ + header->lumps[LUMP_PLANES].fileofs);
	planes_.resize(header->lumps[LUMP_PLANES].filelen / sizeof(*filePlane));

	for (Plane &p : planes_)
	{
		p = Plane(filePlane->normal, filePlane->dist);
		p.setupFastBoundsTest();
		filePlane++;
	}

	// Fogs
	auto fileFogs = (const dfog_t *)(fileData_ + header->lumps[LUMP_FOGS].fileofs);
	fogs_.resize(header->lumps[LUMP_FOGS].filelen / sizeof(*fileFogs));
	auto fileBrushes = (const dbrush_t *)(fileData_ + header->lumps[LUMP_BRUSHES].fileofs);
	const size_t nBrushes = header->lumps[LUMP_BRUSHES].filelen / sizeof(*fileBrushes);
	auto fileBrushSides = (const dbrushside_t *)(fileData_ + header->lumps[LUMP_BRUSHSIDES].fileofs);
	const size_t nBrushSides = header->lumps[LUMP_BRUSHSIDES].filelen / sizeof(*fileBrushSides);

	for (size_t i = 0; i < fogs_.size(); i++)
	{
		Fog &f = fogs_[i];
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
		f.bounds[0][0] = -planes_[planeNum].distance;

		sideNum = firstSide + 1;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[1][0] = planes_[planeNum].distance;

		sideNum = firstSide + 2;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[0][1] = -planes_[planeNum].distance;

		sideNum = firstSide + 3;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[1][1] = planes_[planeNum].distance;

		sideNum = firstSide + 4;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[0][2] = -planes_[planeNum].distance;

		sideNum = firstSide + 5;
		planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
		f.bounds[1][2] = planes_[planeNum].distance;

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
			f.surface = vec4(-vec3(planes_[planeNum].normal), -planes_[planeNum].distance);
		}
	}

	// Lightmaps
	if (header->lumps[LUMP_LIGHTMAPS].filelen > 0)
	{
		const size_t srcDataSize = lightmapSize_ * lightmapSize_ * 3;
		const uint8_t *srcData = &fileData_[header->lumps[LUMP_LIGHTMAPS].fileofs];
		const size_t nLightmaps = header->lumps[LUMP_LIGHTMAPS].filelen / srcDataSize;

		if (nLightmaps)
		{
			// Calculate the smallest square POT atlas size.
			// 1024 is 4MB, 2048 is 16MB. Anything over 1024 is likely to waste a lot of memory for empty space, so use multiple pages in that case.
			const int sr = (int)ceil(sqrtf((float)nLightmaps));
			lightmapAtlasSize_ = 1;

			while (lightmapAtlasSize_ < sr)
				lightmapAtlasSize_ *= 2;

			lightmapAtlasSize_ = std::min(1024, lightmapAtlasSize_ * lightmapSize_);
			nLightmapsPerAtlas_ = (int)pow(lightmapAtlasSize_ / lightmapSize_, 2);
			lightmapAtlases_.resize((size_t)ceil(nLightmaps / (float)nLightmapsPerAtlas_));

			// Pack lightmaps into atlas(es).
			size_t lightmapIndex = 0;

			for (size_t i = 0; i < lightmapAtlases_.size(); i++)
			{
				Image image;
				image.width = lightmapAtlasSize_;
				image.height = lightmapAtlasSize_;
				image.nComponents = 4;
				image.allocMemory();
				int nAtlasedLightmaps = 0;

				for (;;)
				{
					// Expand from 24bpp to 32bpp.
					for (int y = 0; y < lightmapSize_; y++)
					{
						for (int x = 0; x < lightmapSize_; x++)
						{
							const size_t srcOffset = (x + y * lightmapSize_) * 3;
							const int nLightmapsPerDimension = lightmapAtlasSize_ / lightmapSize_;
							const int lightmapX = (nAtlasedLightmaps % nLightmapsPerAtlas_) % nLightmapsPerDimension;
							const int lightmapY = (nAtlasedLightmaps % nLightmapsPerAtlas_) / nLightmapsPerDimension;
							const size_t destOffset = ((lightmapX * lightmapSize_ + x) + (lightmapY * lightmapSize_ + y) * lightmapAtlasSize_) * image.nComponents;
							overbrightenColor(&srcData[srcOffset], &image.memory->data[destOffset]);
							image.memory->data[destOffset + 3] = 0xff;
						}
					}

					nAtlasedLightmaps++;
					lightmapIndex++;
					srcData += srcDataSize;

					if (nAtlasedLightmaps >= nLightmapsPerAtlas_ || lightmapIndex >= nLightmaps)
						break;
				}

				lightmapAtlases_[i] = Texture::create(util::VarArgs("*lightmap%d", (int)i), image, TextureFlags::ClampToEdge);
			}
		}
	}

	// Models
	auto fileModels = (const dmodel_t *)(fileData_ + header->lumps[LUMP_MODELS].fileofs);
	modelDefs_.resize(header->lumps[LUMP_MODELS].filelen / sizeof(*fileModels));

	for (size_t i = 0; i < modelDefs_.size(); i++)
	{
		ModelDef &m = modelDefs_[i];
		const dmodel_t &fm = fileModels[i];
		m.firstSurface = LittleLong(fm.firstSurface);
		m.nSurfaces = LittleLong(fm.numSurfaces);
		m.bounds[0] = vec3(LittleLong(fm.mins[0]), LittleLong(fm.mins[1]), LittleLong(fm.mins[2]));
		m.bounds[1] = vec3(LittleLong(fm.maxs[0]), LittleLong(fm.maxs[1]), LittleLong(fm.maxs[2]));
	}

	// Light grid. Models must be parsed first.
	{
		assert(modelDefs_.size() > 0);
		lump_t &lump = header->lumps[LUMP_LIGHTGRID];

		lightGridInverseSize_.x = 1.0f / lightGridSize_.x;
		lightGridInverseSize_.y = 1.0f / lightGridSize_.y;
		lightGridInverseSize_.z = 1.0f / lightGridSize_.z;

		for (size_t i = 0; i < 3; i++)
		{
			lightGridOrigin_[i] = lightGridSize_[i] * ceil(modelDefs_[0].bounds.min[i] / lightGridSize_[i]);
			const float max = lightGridSize_[i] * floor(modelDefs_[0].bounds.max[i] / lightGridSize_[i]);
			lightGridBounds_[i] = int((max - lightGridOrigin_[i]) / lightGridSize_[i] + 1);
		}

		const int numGridPoints = lightGridBounds_[0] * lightGridBounds_[1] * lightGridBounds_[2];

		if (lump.filelen != numGridPoints * 8)
		{
			interface::PrintWarningf("WARNING: light grid mismatch\n");
		}
		else
		{
			lightGridData_.resize(lump.filelen);
			memcpy(lightGridData_.data(), &fileData_[lump.fileofs], lump.filelen);

			// Deal with overbright bits.
			for (int i = 0; i < numGridPoints; i++)
			{
				overbrightenColor(&lightGridData_[i * 8], &lightGridData_[i * 8]);
				overbrightenColor(&lightGridData_[i * 8 + 3], &lightGridData_[i * 8 + 3]);
			}
		}
	}

	// Materials
	auto fileMaterial = (const dshader_t *)(fileData_ + header->lumps[LUMP_SHADERS].fileofs);
	materials_.resize(header->lumps[LUMP_SHADERS].filelen / sizeof(*fileMaterial));

	for (MaterialDef &m : materials_)
	{
		util::Strncpyz(m.name, fileMaterial->shader, sizeof(m.name));
		m.surfaceFlags = LittleLong(fileMaterial->surfaceFlags);
		m.contentFlags = LittleLong(fileMaterial->contentFlags);
		fileMaterial++;
	}

	// Vertices
	std::vector<Vertex> vertices(header->lumps[LUMP_DRAWVERTS].filelen / sizeof(drawVert_t));
	auto fileDrawVerts = (const drawVert_t *)(fileData_ + header->lumps[LUMP_DRAWVERTS].fileofs);

	for (size_t i = 0; i < vertices.size(); i++)
	{
		Vertex &v = vertices[i];
		const drawVert_t &fv = fileDrawVerts[i];
		v.pos = vec3(LittleFloat(fv.xyz[0]), LittleFloat(fv.xyz[1]), LittleFloat(fv.xyz[2]));
		v.normal = vec3(LittleFloat(fv.normal[0]), LittleFloat(fv.normal[1]), LittleFloat(fv.normal[2]));
		v.texCoord = vec2(LittleFloat(fv.st[0]), LittleFloat(fv.st[1]));
		v.texCoord2 = vec2(LittleFloat(fv.lightmap[0]), LittleFloat(fv.lightmap[1]));

		uint8_t color[3];
		overbrightenColor(fv.color, color);
		v.color = util::ToLinear(vec4(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, fv.color[3] / 255.0f));
	}

	// Indices
	std::vector<uint16_t> indices(header->lumps[LUMP_DRAWINDEXES].filelen / sizeof(int));
	auto fileDrawIndices = (const int *)(fileData_ + header->lumps[LUMP_DRAWINDEXES].fileofs);

	for (size_t i = 0; i < indices.size(); i++)
	{
		indices[i] = LittleLong(fileDrawIndices[i]);
	}

	// Surfaces
	surfaces_.resize(modelDefs_[0].nSurfaces);
	auto fileSurfaces = (const dsurface_t *)(fileData_ + header->lumps[LUMP_SURFACES].fileofs);

	for (size_t i = 0; i < surfaces_.size(); i++)
	{
		Surface &s = surfaces_[i];
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
		s.material = findMaterial(shaderNum, lightmapIndex);
		s.flags = materials_[shaderNum].surfaceFlags;
		s.contentFlags = materials_[shaderNum].contentFlags;

		// We may have a nodraw surface, because they might still need to be around for movement clipping.
		if (s.material->surfaceFlags & SURF_NODRAW || materials_[shaderNum].surfaceFlags & SURF_NODRAW)
		{
			s.type = SurfaceType::Ignore;
		}
		else if (type == MST_PLANAR)
		{
			s.type = SurfaceType::Face;
			const int firstVertex = LittleLong(fs.firstVert);
			const int nVertices = LittleLong(fs.numVerts);
			setSurfaceGeometry(&s, &vertices[firstVertex], nVertices, &indices[LittleLong(fs.firstIndex)], LittleLong(fs.numIndexes), lightmapIndex);

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
			setSurfaceGeometry(&s, &vertices[firstVertex], nVertices, &indices[LittleLong(fs.firstIndex)], LittleLong(fs.numIndexes), lightmapIndex);
		}
		else if (type == MST_PATCH)
		{
			s.type = SurfaceType::Patch;
			s.patch = Patch_Subdivide(LittleLong(fs.patchWidth), LittleLong(fs.patchHeight), &vertices[LittleLong(fs.firstVert)]);
			setSurfaceGeometry(&s, s.patch->verts, s.patch->numVerts, s.patch->indexes, s.patch->numIndexes, lightmapIndex);
		}
		else if (type == MST_FLARE)
		{
			s.type = SurfaceType::Flare;
		}
	}

	// Model surfaces
	for (size_t i = 1; i < modelDefs_.size(); i++)
	{
		const ModelDef &md = modelDefs_[i];
		auto model = std::make_unique<WorldModel>((int)i, md.nSurfaces, md.bounds);

		for (size_t j = 0; j < md.nSurfaces; j++)
		{
			const dsurface_t &fs = fileSurfaces[md.firstSurface + j];
			const int type = LittleLong(fs.surfaceType);
			int lightmapIndex = LittleLong(fs.lightmapNum);
			Material *material = findMaterial(LittleLong(fs.shaderNum), lightmapIndex);

			if (!lightmapAtlases_.empty())
			{
				lightmapIndex = lightmapIndex % nLightmapsPerAtlas_;
			}

			if (type == MST_PLANAR || type == MST_TRIANGLE_SOUP)
			{
				model->addSurface(j, material, &vertices[LittleLong(fs.firstVert)], LittleLong(fs.numVerts), &indices[LittleLong(fs.firstIndex)], LittleLong(fs.numIndexes), lightmapIndex, lightmapAtlasSize_ / lightmapSize_);
			}
			else if (type == MST_PATCH)
			{
				model->addPatchSurface(j, material, LittleLong(fs.patchWidth), LittleLong(fs.patchHeight), &vertices[LittleLong(fs.firstVert)], lightmapIndex, lightmapAtlasSize_ / lightmapSize_);
			}
		}

		model->batchSurfaces();
		g_modelCache->addModel(std::move(model));
	}

	// Leaf surfaces
	auto fileLeafSurfaces = (const int *)(fileData_ + header->lumps[LUMP_LEAFSURFACES].fileofs);
	leafSurfaces_.resize(header->lumps[LUMP_LEAFSURFACES].filelen / sizeof(int));

	for (size_t i = 0; i < leafSurfaces_.size(); i++)
	{
		leafSurfaces_[i] = LittleLong(fileLeafSurfaces[i]);
	}

	// Nodes and leaves
	auto fileNodes = (const dnode_t *)(fileData_ + header->lumps[LUMP_NODES].fileofs);
	auto fileLeaves = (const dleaf_t *)(fileData_ + header->lumps[LUMP_LEAFS].fileofs);
	const size_t nNodes = header->lumps[LUMP_NODES].filelen / sizeof(dnode_t);
	const size_t nLeaves = header->lumps[LUMP_LEAFS].filelen / sizeof(dleaf_t);
	nodes_.resize(nNodes + nLeaves);

	for (size_t i = 0; i < nNodes; i++)
	{
		Node &n = nodes_[i];
		const dnode_t &fn = fileNodes[i];

		n.leaf = false;
		n.bounds[0] = vec3((float)LittleLong(fn.mins[0]), (float)LittleLong(fn.mins[1]), (float)LittleLong(fn.mins[2]));
		n.bounds[1] = vec3((float)LittleLong(fn.maxs[0]), (float)LittleLong(fn.maxs[1]), (float)LittleLong(fn.maxs[2]));
		n.plane = &planes_[LittleLong(fn.planeNum)];

		for (size_t j = 0; j < 2; j++)
		{
			const int c = LittleLong(fn.children[j]);
			n.children[j] = &nodes_[c >= 0 ? c : nNodes + (-1 - c)];
		}
	}

	firstLeaf_ = nNodes;

	for (size_t i = 0; i < nLeaves; i++)
	{
		Node &l = nodes_[firstLeaf_ + i];
		const dleaf_t &fl = fileLeaves[i];

		l.leaf = true;
		l.bounds[0] = vec3((float)LittleLong(fl.mins[0]), (float)LittleLong(fl.mins[1]), (float)LittleLong(fl.mins[2]));
		l.bounds[1] = vec3((float)LittleLong(fl.maxs[0]), (float)LittleLong(fl.maxs[1]), (float)LittleLong(fl.maxs[2]));
		l.cluster = LittleLong(fl.cluster);
		l.area = LittleLong(fl.area);

		if (l.cluster >= nClusters_)
		{
			nClusters_ = l.cluster + 1;
		}

		l.firstSurface = LittleLong(fl.firstLeafSurface);
		l.nSurfaces = LittleLong(fl.numLeafSurfaces);
	}

	// Visibility
	const lump_t &visLump = header->lumps[LUMP_VISIBILITY];

	if (visLump.filelen)
	{
		nClusters_ = *((int *)(fileData_ + visLump.fileofs));
		clusterBytes_ = *((int *)(fileData_ + visLump.fileofs + sizeof(int)));

		// CM_Load should have given us the vis data to share, so we don't need to allocate another copy.
		if (g_externalVisData)
		{
			visData_ = g_externalVisData;
		}
		else
		{
			internalVisData_.resize(visLump.filelen - sizeof(int) * 2);
			memcpy(&internalVisData_[0], fileData_ + visLump.fileofs + sizeof(int) * 2, internalVisData_.size());
			visData_ = &internalVisData_[0];
		}
	}

	// Initialize geometry buffers.
	// Index buffer is initialized on first use, not here.
	for (size_t i = 0; i < currentGeometryBuffer_ + 1; i++)
	{
		vertexBuffers_[i].handle = bgfx::createVertexBuffer(bgfx::makeRef(&vertices_[i][0], uint32_t(vertices_[i].size() * sizeof(Vertex))), Vertex::decl);
	}
}

} // namespace renderer
