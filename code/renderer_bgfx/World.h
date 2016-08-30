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
namespace world {

#define BSP_IDENT	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
// little-endian "IBSP"

#if defined(ENGINE_IOQ3)
#define BSP_VERSION			46
#elif defined(ENGINE_IORTCW)
#define BSP_VERSION			47
#else
#error Engine undefined
#endif

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

struct BatchedSurface
{
	Bounds bounds; // frustum culling only
	Material *material;
	int fogIndex;
	int surfaceFlags;
	int contentFlags;

	/// @remarks Undefined if the material has CPU deforms.
	size_t bufferIndex;

	uint32_t firstIndex;
	uint32_t nIndices;

	/// @remarks Used by CPU deforms only.
	uint32_t firstVertex;

	/// @remarks Used by CPU deforms only.
	uint32_t nVertices;
};

struct CullInfoType
{
	enum
	{
		None = 0,
		Box = 1 << 0,
		Plane = 1 << 1,
		Sphere = 1 << 2
	};
};

struct CullInfo
{
	int type;
	Bounds bounds;
	vec3 localOrigin;
	float radius;
	Plane plane;
};

struct EntityKVP
{
	char key[128];
	char value[128];
};

struct Entity
{
	std::array<EntityKVP, 32> kvps;
	size_t nKvps;

	const char *findValue(const char *key, const char *defaultValue = nullptr) const
	{
		for (size_t i = 0; i < nKvps; i++)
		{
			const EntityKVP &kvp = kvps[i];

			if (!util::Stricmp(kvp.key, key))
				return kvp.value;
		}

		return defaultValue;
	}
};

struct Fog
{
	int originalBrushNumber;
	Bounds bounds;

	unsigned colorInt; // in packed byte format
	float tcScale; // texture coordinate vector scales
	MaterialFogParms parms;

	// for clipping distance in fog when outside
	bool hasSurface;
	vec4 surface;
};

struct MaterialDef
{
	char name[MAX_QPATH];
	int surfaceFlags;
	int contentFlags;
};

struct ModelDef
{
	size_t firstSurface;
	size_t nSurfaces;
	Bounds bounds;
};

struct Node
{
	// common with leaf and node
	bool leaf;
	Bounds bounds;

	// node specific
	Plane *plane;
	Node *children[2];

	// leaf specific
	int cluster;
	int area;
	int firstSurface; // index into leafSurfaces_
	int nSurfaces;
};

enum class SurfaceType
{
	Ignore, /// Ignore this surface when rendering. e.g. material has SURF_NODRAW surfaceFlags 
	Face,
	Mesh,
	Patch,
	Flare
};

struct Surface
{
	~Surface()
	{
		if (patch)
			Patch_Free(patch);
	}

	SurfaceType type;
	Material *material;
	int fogIndex;
	int flags; // SURF *
	int contentFlags;
	std::vector<uint16_t> indices;

	/// Which geometry buffer to use.
	size_t bufferIndex;

	CullInfo cullinfo;

	// SurfaceType::Patch
	Patch *patch = nullptr;

	/// Used at runtime to avoid adding duplicate visible surfaces.
	int duplicateId = -1;

	/// Used at runtime to avoid processing surfaces multiple times when adding a decal.
	int decalDuplicateId = -1;

	/// @remarks Used by CPU deforms only.
	uint32_t firstVertex;

	/// @remarks Used by CPU deforms only.
	uint32_t nVertices;
};

static const size_t s_maxWorldGeometryBuffers = 8;

enum class VisibilityMethod
{
	PVS,
	CameraFrustum
};

struct Visibility
{
	struct Portal
	{
		const renderer::Entity *entity;
		bool isMirror;
		Plane plane;
		Surface *surface;
	};

	struct Reflective
	{
		Plane plane;
		Surface *surface;
	};

	/// Visible surfaces batched by material.
	std::vector<BatchedSurface> batchedSurfaces;

	/// The merged bounds of all visible leaves.
	Bounds bounds;

	/// Portal surfaces visible to the camera.
	std::vector<Portal> cameraPortalSurfaces;

	/// Reflective surfaces visible to the camera.
	std::vector<Reflective> cameraReflectiveSurfaces;

	std::vector<Vertex> cpuDeformVertices;
	std::vector<uint16_t> cpuDeformIndices;

	DynamicIndexBuffer indexBuffers[s_maxWorldGeometryBuffers];

	/// Temporary index data populated at runtime when surface visibility changes.
	std::vector<uint16_t> indices[s_maxWorldGeometryBuffers];

	/// The camera leaf from the last UpdateVisibility call.
	/// @remarks Visibility is only recalculated if the camera leaf cluster or area mask changes.
	Node *lastCameraLeaf = nullptr;

	/// The area mask from the last UpdateVisibility call.
	/// @remarks Visibility is only recalculated if the camera leaf cluster or area mask changes.
	uint8_t lastAreaMask[MAX_MAP_AREA_BYTES];

	VisibilityMethod method;

	/// Portal surface visible to the PVS.
	std::vector<Surface *> portalSurfaces;

	/// Reflective surfaces visible to the PVS.
	std::vector<Surface *> reflectiveSurfaces;

	std::vector<SkySurface> skySurfaces;

	/// Surfaces visible from the camera leaf cluster.
	std::vector<Surface *> surfaces;
};

struct World
{
	char name[MAX_QPATH]; // ie: maps/tim_dm2.bsp
	char baseName[MAX_QPATH]; // ie: tim_dm2

	std::vector<char> entityString;
	char *entityParsePoint = nullptr;
	std::vector<Entity> entities;
	std::vector<Fog> fogs;
	const int lightmapSize = 128;
	vec2i lightmapAtlasSize; // In cells. e.g. 2x2 is 256x256 (lightmapSize).
	std::vector<Texture *> lightmapAtlases;
	int nLightmapsPerAtlas;
	vec3 lightGridSize = { 64, 64, 128 };
	vec3 lightGridInverseSize;
	std::vector<uint8_t> lightGridData;
	vec3 lightGridOrigin;
	vec3i lightGridBounds;
	std::vector<MaterialDef> materials;
	std::vector<ModelDef> modelDefs;
	std::vector<Plane> planes;

	/// All model surfaces.
	std::vector<Surface> surfaces;

	VertexBuffer vertexBuffers[s_maxWorldGeometryBuffers];

	/// Vertex data populated at load time.
	std::vector<Vertex> vertices[s_maxWorldGeometryBuffers];

	/// Incremented when a surface won't fit in the current geometry buffer (16-bit indices).
	size_t currentGeometryBuffer = 0;

	std::vector<Node> nodes;
	std::vector<int> leafSurfaces;

	/// Index into nodes_ for the first leaf.
	size_t firstLeaf;

	int nClusters;
	int clusterBytes;
	const uint8_t *visData = nullptr;
	std::vector<uint8_t> internalVisData;
	std::array<Visibility, (int)VisibilityId::Num> visibility;

	/// Used at runtime to avoid adding duplicate visible surfaces.
	/// @remarks Incremented once everytime UpdateVisibility is called.
	int duplicateSurfaceId = 0;

	int decalDuplicateSurfaceId = 0;

	// frustum culling
	std::vector<BatchedSurface> batchedSurfaces;
	std::vector<Vertex> cpuDeformVertices;
	std::vector<uint16_t> cpuDeformIndices;
	IndexBuffer indexBuffers[s_maxWorldGeometryBuffers];
	std::vector<SkySurface> skySurfaces;
};

extern std::unique_ptr<World> s_world;

Node *LeafFromPosition(vec3 pos);
int GetNumModels();
int GetNumSurfaces(int modelIndex);
const Surface &GetSurface(int modelIndex, int surfaceIndex);
int GetNumVertexBuffers();
const std::vector<Vertex> &GetVertexBuffer(int index);

} // namespace world
} // namespace renderer
