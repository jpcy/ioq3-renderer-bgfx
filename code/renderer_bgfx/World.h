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

static const int MAX_VERTS_ON_POLY = 64;

class WorldModel : public Model
{
public:
	WorldModel(int index, size_t nSurfaces, Bounds bounds);
	bool load() override { return true; }
	Bounds getBounds() const override { return bounds_; }
	Transform getTag(const char *name, int frame) const override { return Transform(); }
	bool isCulled(Entity *entity, const Frustum &cameraFrustum) const override;
	void render(const mat3 &scenRotation, DrawCallList *drawCallList, Entity *entity) override;
	void addPatchSurface(size_t index, Material *material, int width, int height, const Vertex *points, int lightmapIndex, int nLightmapTilesPerDimension);
	void addSurface(size_t index, Material *material, const Vertex *vertices, size_t nVertices, const uint16_t *indices, size_t nIndices, int lightmapIndex, int nLightmapTilesPerDimension);
	void batchSurfaces();

private:
	struct Surface
	{
		Material *material;
		uint32_t firstIndex;
		uint32_t nIndices;
	};

	Bounds bounds_;
	std::vector<Surface> surfaces_;
	VertexBuffer vertexBuffer_;
	uint32_t nVertices_;
	IndexBuffer indexBuffer_;

	/// @remarks Not used after surfaces are batched.
	struct TempSurface
	{
		/// @remarks Temp surfaces with no material are ignored.
		Material *material = nullptr;

		uint32_t firstVertex;
		uint32_t nVertices;
		uint32_t firstIndex;
		uint32_t nIndices;
		bool batched;
	};

	std::vector<TempSurface> tempSurfaces_;
	std::vector<Vertex> tempVertices_;
	std::vector<uint16_t> tempIndices_;
};

class World
{
public:
	const Texture *getLightmap(size_t index) const;
	bool getEntityToken(char *buffer, int size);
	bool hasLightGrid() const;
	void sampleLightGrid(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir) const;
	bool inPvs(vec3 position1, vec3 position2);
	int findFogIndex(vec3 position, float radius) const;
	int findFogIndex(const Bounds &bounds) const;
	void calculateFog(int fogIndex, const mat4 &modelMatrix, const mat4 &modelViewMatrix, vec3 cameraPosition, vec3 localViewPosition, const mat3 &cameraRotation, vec4 *fogColor, vec4 *fogDistance, vec4 *fogDepth, float *eyeT) const;
	int markFragments(int numPoints, const vec3 *points, const vec3 projection, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);
	Bounds getBounds() const;
	Bounds getBounds(uint8_t visCacheId) const;
	size_t getNumSkies(uint8_t visCacheId) const;
	void getSky(uint8_t visCacheId, size_t index, Material **material, const std::vector<Vertex> **vertices) const;
	bool calculatePortalCamera(uint8_t visCacheId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, const std::vector<Entity> &entities, vec3 *pvsPosition, Transform *portalCamera, bool *isMirror, Plane *portalPlane) const;
	bool calculateReflectionCamera(uint8_t visCacheId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, Transform *camera, Plane *plane);
	void renderPortal(uint8_t visCacheId, DrawCallList *drawCallList);
	void renderReflective(uint8_t visCacheId, DrawCallList *drawCallList);
	void load(const char *name);
	uint8_t createVisCache();
	void updateVisCache(uint8_t visCacheId, vec3 cameraPosition, const uint8_t *areaMask);
	void render(uint8_t visCacheId, DrawCallList *drawCallList, const mat3 &sceneRotation);

private:
	static const size_t maxWorldGeometryBuffers_ = 8;

	enum class SurfaceType
	{
		Ignore, /// Ignore this surface when rendering. e.g. material has SURF_NODRAW surfaceFlags 
		Face,
		Mesh,
		Patch,
		Flare
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
		int flags; // SURF_*
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

	struct BatchedSurface
	{
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

	struct VisCache
	{
		static const size_t maxSkies = 4;
		size_t nSkies = 0;
		Material *skyMaterials[maxSkies];
		std::vector<Vertex> skyVertices[maxSkies];

		Node *lastCameraLeaf = nullptr;
		uint8_t lastAreaMask[MAX_MAP_AREA_BYTES];

		/// The merged bounds of all visible leaves.
		Bounds bounds;

		/// Surfaces visible from the camera leaf cluster.
		std::vector<Surface *> surfaces;

		/// Visible surfaces batched by material.
		std::vector<BatchedSurface> batchedSurfaces;

		DynamicIndexBuffer indexBuffers[maxWorldGeometryBuffers_];

		/// Temporary index data populated at runtime when surface visibility changes.
		std::vector<uint16_t> indices[maxWorldGeometryBuffers_];

		/// Portal surface visible to the PVS.
		std::vector<Surface *> portalSurfaces;

		struct Portal
		{
			const Entity *entity;
			bool isMirror;
			Plane plane;
			Surface *surface;
		};

		/// Portal surfaces visible to the camera.
		std::vector<Portal> cameraPortalSurfaces;

		std::vector<Vertex> cpuDeformVertices;
		std::vector<uint16_t> cpuDeformIndices;

		/// Reflective surfaces visible to the PVS.
		std::vector<Surface *> reflectiveSurfaces;

		struct Reflective
		{
			Plane plane;
			Surface *surface;
		};

		/// Reflective surfaces visible to the Camera.
		std::vector<Reflective> cameraReflectiveSurfaces;
	};

	uint8_t *fileData_;
	char name_[MAX_QPATH]; // ie: maps/tim_dm2.bsp
	char baseName_[MAX_QPATH]; // ie: tim_dm2

	std::vector<char> entityString_;
	char *entityParsePoint_ = nullptr;

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

	std::vector<Fog> fogs_;
	const int lightmapSize_ = 128;
	int lightmapAtlasSize_;
	std::vector<const Texture *> lightmapAtlases_;
	int nLightmapsPerAtlas_;
	vec3 lightGridSize_ = { 64, 64, 128 };
	vec3 lightGridInverseSize_;
	std::vector<uint8_t> lightGridData_;
	vec3 lightGridOrigin_;
	vec3i lightGridBounds_;

	struct MaterialDef
	{
		char name[MAX_QPATH];
		int surfaceFlags;
		int contentFlags;
	};

	std::vector<MaterialDef> materials_;
	std::vector<Plane> planes_;

	struct ModelDef
	{
		size_t firstSurface;
		size_t nSurfaces;
		Bounds bounds;
	};

	std::vector<ModelDef> modelDefs_;

	/// First model surfaces.
	std::vector<Surface> surfaces_;

	VertexBuffer vertexBuffers_[maxWorldGeometryBuffers_];

	/// Vertex data populated at load time.
	std::vector<Vertex> vertices_[maxWorldGeometryBuffers_];

	/// Incremented when a surface won't fit in the current geometry buffer (16-bit indices).
	size_t currentGeometryBuffer_ = 0;

	std::vector<Node> nodes_;
	std::vector<int> leafSurfaces_;

	/// Index into nodes_ for the first leaf.
	size_t firstLeaf_;

	int nClusters_;
	int clusterBytes_;
	const uint8_t *visData_ = nullptr;
	std::vector<uint8_t> internalVisData_;
	std::vector<std::unique_ptr<VisCache>> visCaches_;

	/// Used at runtime to avoid adding duplicate visible surfaces.
	/// @remarks Incremented once everytime updateVisCache is called.
	int duplicateSurfaceId_ = 0;

	int decalDuplicateSurfaceId_ = 0;

	static bool surfaceCompare(const Surface *s1, const Surface *s2);
	static void overbrightenColor(const uint8_t *in, uint8_t *out);
	void setSurfaceGeometry(Surface *surface, const Vertex *vertices, int nVertices, const uint16_t *indices, size_t nIndices, int lightmapIndex);
	void appendSkySurfaceGeometry(uint8_t visCacheId, size_t skyIndex, const Surface &surface);
	Material *findMaterial(int materialIndex, int lightmapIndex);
	Node *leafFromPosition(vec3 pos);
	void boxSurfaces_recursive(Node *node, Bounds bounds, Surface **list, int listsize, int *listlength, vec3 dir);
	void addMarkFragments(int numClipPoints, vec3 clipPoints[2][MAX_VERTS_ON_POLY], int numPlanes, const vec3 *normals, const float *dists, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer, int *returnedPoints, int *returnedFragments);
};

} // namespace renderer
