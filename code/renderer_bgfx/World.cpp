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

#define MAX_VERTS_ON_POLY		64

#define MARKER_OFFSET			0	// 1

namespace renderer {

static const size_t maxWorldGeometryBuffers = 8;

enum class SurfaceType
{
	Ignore, /// Ignore this surface when rendering. e.g. material has SURF_NODRAW surfaceFlags 
	Face,
	Mesh,
	Patch,
	Flare
};

#define CULLINFO_NONE   0
#define CULLINFO_BOX    1
#define CULLINFO_SPHERE 2
#define CULLINFO_PLANE  4

struct CullInfo
{
	int type;
	Bounds bounds;
	vec3 localOrigin;
	float radius;
	cplane_t plane;
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
};

struct Node
{
	// common with leaf and node
	bool leaf;
	Bounds bounds;

	// node specific
	cplane_t *plane;
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

	/// @remarks Undefined if the material has CPU deforms.
	size_t bufferIndex;

	uint32_t firstIndex;
	uint32_t nIndices;
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

	DynamicIndexBuffer indexBuffers[maxWorldGeometryBuffers];

	/// Temporary index data populated at runtime when surface visibility changes.
	std::vector<uint16_t> indices[maxWorldGeometryBuffers];

	/// Visible portal surface.
	std::vector<Surface *> portalSurfaces;

	std::vector<Vertex> cpuDeformVertices;
	std::vector<uint16_t> cpuDeformIndices;
};

static vec2 AtlasTexCoord(vec2 uv, int index, int nTilesPerDimension)
{
	const int tileX = index % nTilesPerDimension;
	const int tileY = index / nTilesPerDimension;
	vec2 result;
	result.u = (tileX / (float)nTilesPerDimension) + (uv.u / (float)nTilesPerDimension);
	result.v = (tileY / (float)nTilesPerDimension) + (uv.v / (float)nTilesPerDimension);
	return result;
}

class WorldModel : public Model
{
public:
	WorldModel(int index, size_t nSurfaces, Bounds bounds) : tempSurfaces_(nSurfaces), bounds_(bounds)
	{
		Com_sprintf(name_, sizeof(name_), "*%d", index);
	}

	bool load() override
	{
		return true;
	}

	Bounds getBounds() const override
	{
		return bounds_;
	}

	Transform getTag(const char *name, int frame) const override
	{
		return Transform();
	}

	bool isCulled(Entity *entity, const Frustum &cameraFrustum) const override
	{
		return cameraFrustum.clipBounds(bounds_, mat4::transform(entity->e.axis, entity->e.origin)) == Frustum::ClipResult::Outside;
	}

	void render(DrawCallList *drawCallList, Entity *entity) override
	{
		assert(drawCallList);
		assert(entity);
		const auto modelMatrix = mat4::transform(entity->e.axis, entity->e.origin);

		for (auto &surface : surfaces_)
		{
			DrawCall dc;
			dc.entity = entity;
			dc.material = surface.material;
			dc.modelMatrix = modelMatrix;
			dc.vb.type = dc.ib.type = DrawCall::BufferType::Static;
			dc.vb.staticHandle = vertexBuffer_.handle;
			dc.vb.nVertices = nVertices_;
			dc.ib.staticHandle = indexBuffer_.handle;
			dc.ib.firstIndex = surface.firstIndex;
			dc.ib.nIndices = surface.nIndices;
			drawCallList->push_back(dc);
		}
	}

	void addPatchSurface(size_t index, Material *material, int width, int height, const Vertex *points, int lightmapIndex, int nLightmapTilesPerDimension)
	{
		Patch *patch = Patch_Subdivide(width, height, points);
		addSurface(index, material, patch->verts, patch->numVerts, patch->indexes, patch->numIndexes, lightmapIndex, nLightmapTilesPerDimension);
		Patch_Free(patch);
	}

	void addSurface(size_t index, Material *material, const Vertex *vertices, size_t nVertices, const uint16_t *indices, size_t nIndices, int lightmapIndex, int nLightmapTilesPerDimension)
	{
		// Create a temp surface.
		auto &ts = tempSurfaces_[index];
		ts.material = material;
		ts.firstVertex = (uint32_t)tempVertices_.size();
		ts.nVertices = (uint32_t)nVertices;
		ts.firstIndex = (uint32_t)tempIndices_.size();
		ts.nIndices = (uint32_t)nIndices;
		ts.batched = false;

		// Append the geometry.
		tempVertices_.resize(tempVertices_.size() + nVertices);
		memcpy(&tempVertices_[ts.firstVertex], vertices, nVertices * sizeof(*vertices));

		for (size_t i = 0; i < nVertices; i++)
		{
			Vertex *v = &tempVertices_[ts.firstVertex + i];
			v->texCoord2 = AtlasTexCoord(v->texCoord2, lightmapIndex, nLightmapTilesPerDimension);
		}

		tempIndices_.resize(tempIndices_.size() + nIndices);
		memcpy(&tempIndices_[ts.firstIndex], indices, nIndices * sizeof(*indices));
	}

	void batchSurfaces()
	{
		if (tempVertices_.empty() || tempIndices_.empty())
			return;

		// Allocate buffers for the batched geometry.
		auto verticesMem = bgfx::alloc(uint32_t(sizeof(Vertex) * tempVertices_.size()));
		auto vertices = (Vertex *)verticesMem->data;
		auto indicesMem = bgfx::alloc(uint32_t(sizeof(uint16_t) * tempIndices_.size()));
		auto indices = (uint16_t *)indicesMem->data;
		uint32_t currentVertex = 0, currentIndex = 0;

		for (;;)
		{
			Material *material = nullptr;

			// Get the material from the first temp surface that hasn't been batched.
			for (const auto &ts : tempSurfaces_)
			{
				if (!ts.material)
					continue;

				if (!ts.batched)
				{
					material = ts.material;
					break;
				}
			}

			// Stop when all temp surfaces are batched.
			if (!material)
				break;

			// Find a batched surface with the same material.
			Surface *surface = nullptr;

			for (auto &s : surfaces_)
			{
				if (s.material == material)
				{
					surface = &s;
					break;
				}
			}

			// If not found, create one.
			if (!surface)
			{
				Surface s;
				s.material = material;
				s.firstIndex = currentIndex;
				s.nIndices = 0;
				surfaces_.push_back(s);
				surface = &surfaces_.back();
			}

			// Batch all temp surfaces with this material.
			for (auto &ts : tempSurfaces_)
			{
				if (!ts.material || ts.material != material)
					continue;

				memcpy(&vertices[currentVertex], &tempVertices_[ts.firstVertex], sizeof(Vertex) * ts.nVertices);
				
				for (size_t i = 0; i < ts.nIndices; i++)
				{
					// Make indices absolute.
					indices[currentIndex + i] = currentVertex + tempIndices_[ts.firstIndex + i];
				}

				surface->nIndices += ts.nIndices;
				currentVertex += ts.nVertices;
				currentIndex += ts.nIndices;
				ts.batched = true;
			}
		}

		// Create vertex and index buffers.
		vertexBuffer_.handle = bgfx::createVertexBuffer(verticesMem, Vertex::decl);
		nVertices_ = (uint32_t)tempVertices_.size();
		indexBuffer_.handle = bgfx::createIndexBuffer(indicesMem);

		// Clear temp state.
		tempSurfaces_.clear();
		tempVertices_.clear();
		tempIndices_.clear();
	}

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

static vec3 MirroredPoint(vec3 in, const Transform &surface, const Transform &camera)
{
	const vec3 local = in - surface.position;
	vec3 transformed;

	for (size_t i = 0; i < 3; i++)
	{
		transformed += camera.rotation[i] * vec3::dotProduct(local, surface.rotation[i]);
	}

	return transformed + camera.position;
}

static vec3 MirroredVector(vec3 in, const Transform &surface, const Transform &camera)
{
	vec3 transformed;

	for (size_t i = 0; i < 3; i++)
	{
		transformed += camera.rotation[i] * vec3::dotProduct(in, surface.rotation[i]);
	}

	return transformed;
}

/*
=============
R_ChopPolyBehindPlane

Out must have space for two more vertexes than in
=============
*/
#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

static void R_ChopPolyBehindPlane(int numInPoints, const vec3 *inPoints, int *numOutPoints, vec3 *outPoints, vec3 normal, float dist, float epsilon)
{
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

class World
{
public:
	const Texture *getLightmap(size_t index) const
	{
		return index < lightmapAtlases_.size() ? lightmapAtlases_[index] : nullptr;
	}

	bool getEntityToken(char *buffer, int size)
	{
		const char *s = COM_Parse(&entityParsePoint_);
		Q_strncpyz(buffer, s, size);

		if (!entityParsePoint_ && !s[0])
		{
			entityParsePoint_ = entityString_.data();
			return false;
		}

		return true;
	}

	bool hasLightGrid() const
	{
		return !lightGridData_.empty();
	}

	void sampleLightGrid(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir) const
	{
		assert(ambientLight);
		assert(directedLight);
		assert(lightDir);
		assert(hasLightGrid()); // false with -nolight maps

		vec3 lightPosition = position - lightGridOrigin_;
		int pos[3];
		float frac[3];

		for (size_t i = 0; i < 3; i++)
		{
			float v = lightPosition[i] * lightGridInverseSize_[i];
			pos[i] = floor(v),
			frac[i] = v - pos[i];
			pos[i] = math::Clamped(pos[i], 0, lightGridBounds_[i] - 1);
		}

		*ambientLight = vec3::empty;
		*directedLight = vec3::empty;
		vec3 direction;

		// Trilerp the light value.
		int gridStep[3];
		gridStep[0] = 8;
		gridStep[1] = 8 * lightGridBounds_[0];
		gridStep[2] = 8 * lightGridBounds_[0] * lightGridBounds_[1];
		const uint8_t *gridData = &lightGridData_[pos[0] * gridStep[0] + pos[1] * gridStep[1] + pos[2] * gridStep[2]];

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
					if (pos[j] + 1 > lightGridBounds_[j] - 1)
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

			(*ambientLight)[0] += factor * data[0] * g_overbrightFactor;
			(*ambientLight)[1] += factor * data[1] * g_overbrightFactor;
			(*ambientLight)[2] += factor * data[2] * g_overbrightFactor;
			(*directedLight)[0] += factor * data[3] * g_overbrightFactor;
			(*directedLight)[1] += factor * data[4] * g_overbrightFactor;
			(*directedLight)[2] += factor * data[5] * g_overbrightFactor;

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

	bool inPvs(vec3 position1, vec3 position2)
	{
		Node *leaf = leafFromPosition(position1);
		byte *vis = ri.CM_ClusterPVS(leaf->cluster);
		leaf = leafFromPosition(position2);
		return ((vis[leaf->cluster >> 3] & (1 << (leaf->cluster & 7))) != 0);
	}

	int findFogIndex(vec3 position, float radius) const
	{
		for (int i = 0; i < fogs_.size(); i++)
		{
			auto &fog = fogs_[i];
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

	int findFogIndex(const Bounds &bounds) const
	{
		for (int i = 0; i < fogs_.size(); i++)
		{
			if (Bounds::intersect(bounds, fogs_[i].bounds))
				return i;
		}

		return -1;
	}

	void calculateFog(int fogIndex, const mat4 &modelMatrix, const mat4 &modelViewMatrix, vec3 cameraPosition, vec3 localViewPosition, const mat3 &cameraRotation, vec4 *fogColor, vec4 *fogDistance, vec4 *fogDepth, float *eyeT) const
	{
		assert(fogIndex != -1);
		assert(fogDistance);
		assert(fogDepth);
		assert(eyeT);
		auto &fog = fogs_[fogIndex];

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

	int markFragments(int numPoints, const vec3 *points, const vec3 projection, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)
	{
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
		
		decalDuplicateSurfaceId_++; // double check prevention
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
		boxSurfaces_recursive(&nodes_[0], bounds, surfaces, 64, &numsurfaces, projectionDir);
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
						auto dv = surface->patch->verts + m * surface->patch->width + n;
						clipPoints[0][0] = dv[0].pos + dv[0].normal * MARKER_OFFSET;
						clipPoints[0][1] = dv[surface->patch->width].pos + dv[surface->patch->width].normal * MARKER_OFFSET;
						clipPoints[0][2] = dv[1].pos + dv[1].normal * MARKER_OFFSET;

						// check the normal of this triangle
						vec3 v1 = clipPoints[0][0] - clipPoints[0][1];
						vec3 v2 = clipPoints[0][2] - clipPoints[0][1];
						vec3 normal = vec3::crossProduct(v1, v2);
						normal.normalizeFast();

						if (vec3::dotProduct(normal, projectionDir) < -0.1)
						{
							// add the fragments of this triangle
							addMarkFragments(numClipPoints, clipPoints, numPlanes, normals, dists, maxPoints, pointBuffer, maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments);

							if (returnedFragments == maxFragments)
								return returnedFragments;	// not enough space for more fragments
						}

						clipPoints[0][0] = dv[1].pos + dv[1].normal * MARKER_OFFSET;
						clipPoints[0][1] = dv[surface->patch->width].pos + dv[surface->patch->width].normal * MARKER_OFFSET;
						clipPoints[0][2] = dv[surface->patch->width + 1].pos + dv[surface->patch->width + 1].normal * MARKER_OFFSET;

						// check the normal of this triangle
						v1 = clipPoints[0][0] - clipPoints[0][1];
						v2 = clipPoints[0][2] - clipPoints[0][1];
						normal = vec3::crossProduct(v1, v2);
						normal.normalizeFast();

						if (vec3::dotProduct(normal, projectionDir) < -0.05)
						{
							// add the fragments of this triangle
							addMarkFragments(numClipPoints, clipPoints, numPlanes, normals, dists, maxPoints, pointBuffer, maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments);

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

				for (k = 0, tri = &surface->indices[0]; k < surface->indices.size(); k += 3, tri += 3)
				{
					for (j = 0; j < 3; j++)
					{
						clipPoints[0][j] = vertices_[surface->bufferIndex][tri[j]].pos + vec3(surface->cullinfo.plane.normal) * MARKER_OFFSET;
					}

					// add the fragments of this face
					addMarkFragments(3, clipPoints, numPlanes, normals, dists, maxPoints, pointBuffer, maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments);

					if (returnedFragments == maxFragments)
						return returnedFragments;	// not enough space for more fragments
				}
			}
			else if (surface->type == SurfaceType::Mesh)
			{
				uint16_t *tri;

				for (k = 0, tri = &surface->indices[0]; k < surface->indices.size(); k += 3, tri += 3)
				{
					for (j = 0; j < 3; j++)
					{
						clipPoints[0][j] = vertices_[surface->bufferIndex][tri[j]].pos + vertices_[surface->bufferIndex][tri[j]].normal * MARKER_OFFSET;
					}

					// add the fragments of this face
					addMarkFragments(3, clipPoints, numPlanes, normals, dists, maxPoints, pointBuffer, maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments);

					if (returnedFragments == maxFragments)
						return returnedFragments;	// not enough space for more fragments
				}
			}
		}

		return returnedFragments;
	}

	Bounds getBounds(uint8_t visCacheId) const
	{
		return visCaches_[visCacheId]->bounds;
	}

	size_t getNumSkies(uint8_t visCacheId) const
	{
		return visCaches_[visCacheId]->nSkies;
	}

	void getSky(uint8_t visCacheId, size_t index, Material **material, const std::vector<Vertex> **vertices) const
	{
		if (material)
		{
			*material = visCaches_[visCacheId]->skyMaterials[index];
		}

		if (vertices)
		{
			*vertices = &visCaches_[visCacheId]->skyVertices[index];
		}
	}

	size_t getNumPortalSurfaces(uint8_t visCacheId) const
	{
		return visCaches_[visCacheId]->portalSurfaces.size();
	}

	bool calculatePortalCamera(uint8_t visCacheId, size_t portalSurfaceIndex, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, const std::vector<Entity> &entities, vec3 *pvsPosition, Transform *portalCamera, bool *isMirror, vec4 *portalPlane) const
	{
		assert(pvsPosition);
		assert(portalCamera);
		assert(isMirror);
		assert(portalPlane);
		const Surface *portalSurface = visCaches_[visCacheId]->portalSurfaces[portalSurfaceIndex];
		uint32_t pointOr = 0, pointAnd = (uint32_t)~0;

		for (size_t i = 0; i < portalSurface->indices.size(); i++)
		{
			uint32_t pointFlags = 0;
			const vec4 clip = mvp.transform(vec4(vertices_[portalSurface->bufferIndex][portalSurface->indices[i]].pos, 1));

			for (size_t j = 0; j < 3; j++)
			{
				if (clip[j] >= clip.w)
				{
					pointFlags |= (1 << (j * 2));
				}
				else if (clip[j] <= -clip.w)
				{
					pointFlags |= (1 << (j * 2 + 1));
				}
			}

			pointAnd &= pointFlags;
			pointOr |= pointFlags;
		}

		// Trivially reject.
		if (pointAnd)
			return false;

		// Determine if this surface is backfaced and also determine the distance to the nearest vertex so we can cull based on portal range.
		// Culling based on vertex distance isn't 100% correct (we should be checking for range to the surface), but it's good enough for the types of portals we have in the game right now.
		size_t nTriangles = portalSurface->indices.size() / 3;
		float shortest = 100000000;

		for (size_t i = 0; i < portalSurface->indices.size(); i += 3)
		{
			const Vertex &vertex = vertices_[portalSurface->bufferIndex][portalSurface->indices[i]];
			const vec3 normal = vertex.pos - mainCameraPosition;
			const float length = normal.lengthSquared(); // lose the sqrt

			if (length < shortest)
			{
				shortest = length;
			}

			if (vec3::dotProduct(normal, vertex.normal) >= 0)
			{
				nTriangles--;
			}
		}

		if (nTriangles == 0)
			return false;

		// Calculate surface plane.
		cplane_t plane;

		if (portalSurface->indices.size() >= 3)
		{
			const vec3 v1(vertices_[portalSurface->bufferIndex][portalSurface->indices[0]].pos);
			const vec3 v2(vertices_[portalSurface->bufferIndex][portalSurface->indices[1]].pos);
			const vec3 v3(vertices_[portalSurface->bufferIndex][portalSurface->indices[2]].pos);
			VectorCopy(vec3::crossProduct(v3 - v1, v2 - v1).normal(), plane.normal);
			plane.dist = vec3::dotProduct(v1, plane.normal);
		}
		else
		{
			Com_Memset(&plane, 0, sizeof(plane));
			plane.normal[0] = 1;	
		}

		// Locate the portal entity closest to this plane.
		// Origin will be the origin of the portal, oldorigin will be the origin of the camera.
		const Entity *portalEntity = nullptr;

		for (size_t i = 0; i < entities.size(); i++) 
		{
			const Entity &entity = entities[i];
			
			if (entity.e.reType == RT_PORTALSURFACE)
			{
				const float d = vec3::dotProduct(entity.e.origin, plane.normal) - plane.dist;

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
			return false;

		// Mirrors don't do a fade over distance (although they could).
		*isMirror = (vec3(portalEntity->e.origin) == vec3(portalEntity->e.oldorigin));
		
		if (!*isMirror && shortest > portalSurface->material->portalRange * portalSurface->material->portalRange)
			return false;

		// Portal surface is visible.
		// Calculate portal camera transform.
		Transform surfaceTransform, cameraTransform;
		surfaceTransform.rotation[0] = plane.normal;
		surfaceTransform.rotation[1] = surfaceTransform.rotation[0].perpendicular();
		surfaceTransform.rotation[2] = vec3::crossProduct(surfaceTransform.rotation[0], surfaceTransform.rotation[1]);

		// Get the PVS position from the entity.
		*pvsPosition = portalEntity->e.oldorigin;

		// If the entity is just a mirror, don't use as a camera point.
		if (*isMirror)
		{
			surfaceTransform.position = vec3(plane.normal) * plane.dist;
			cameraTransform.position = surfaceTransform.position;
			cameraTransform.rotation[0] = -surfaceTransform.rotation[0];
			cameraTransform.rotation[1] = surfaceTransform.rotation[1];
			cameraTransform.rotation[2] = surfaceTransform.rotation[2];
		}
		else
		{
			// Project the origin onto the surface plane to get an origin point we can rotate around.
			const float d = vec3::dotProduct(portalEntity->e.origin, plane.normal) - plane.dist;
			surfaceTransform.position = vec3(portalEntity->e.origin) + surfaceTransform.rotation[0] * -d;

			// Now get the camera position and rotation.
			cameraTransform.position = portalEntity->e.oldorigin;
			cameraTransform.rotation[0] = -vec3(portalEntity->e.axis[0]);
			cameraTransform.rotation[1] = -vec3(portalEntity->e.axis[1]);
			cameraTransform.rotation[2] = portalEntity->e.axis[2];

			// Optionally rotate.
			if (portalEntity->e.oldframe || portalEntity->e.skinNum)
			{
				float d;

				if (portalEntity->e.oldframe)
				{
					// If a speed is specified.
					if (portalEntity->e.frame)
					{
						// Continuous rotate
						d = main::GetFloatTime() * portalEntity->e.frame;
					}
					else
					{
						// Bobbing rotate, with skinNum being the rotation offset.
						d = portalEntity->e.skinNum + sin(main::GetFloatTime()) * 4;
					}
				}
				else if (portalEntity->e.skinNum)
				{
					d = portalEntity->e.skinNum;
				}

				cameraTransform.rotation[1] = cameraTransform.rotation[1].rotatedAroundDirection(cameraTransform.rotation[0], d);
				cameraTransform.rotation[2] = vec3::crossProduct(cameraTransform.rotation[0], cameraTransform.rotation[1]);
			}
		}

		portalCamera->position = MirroredPoint(mainCameraPosition, surfaceTransform, cameraTransform);
		portalCamera->rotation[0] = MirroredVector(mainCameraRotation[0], surfaceTransform, cameraTransform);
		portalCamera->rotation[1] = MirroredVector(mainCameraRotation[1], surfaceTransform, cameraTransform);
		portalCamera->rotation[2] = MirroredVector(mainCameraRotation[2], surfaceTransform, cameraTransform);
		*portalPlane = vec4(-cameraTransform.rotation[0], vec3::dotProduct(cameraTransform.position, -cameraTransform.rotation[0]));
		return true;
	}

	void load(const char *name)
	{
		Q_strncpyz(name_, name, sizeof(name_));
		Q_strncpyz(baseName_, COM_SkipPath(name_), sizeof(baseName_));
		COM_StripExtension(baseName_, baseName_, sizeof(baseName_));

		ReadOnlyFile file(name_);

		if (!file.isValid())
		{
			ri.Error(ERR_DROP, "%s not found", name_);
			return;
		}

		fileData_ = file.getData();
		loadFromBspFile();
	}

	uint8_t createVisCache()
	{
		visCaches_.push_back(std::make_unique<VisCache>());
		return uint8_t(visCaches_.size() - 1);
	}

	void updateVisCache(uint8_t visCacheId, vec3 cameraPosition, const uint8_t *areaMask)
	{
		assert(areaMask);
		auto &visCache = visCaches_[visCacheId];

		// Get the PVS for the camera leaf cluster.
		auto cameraLeaf = leafFromPosition(cameraPosition);

		// Build a list of visible surfaces.
		// Don't need to refresh visible surfaces if the camera cluster or the area bitmask haven't changed.
		if (visCache->lastCameraLeaf == nullptr || visCache->lastCameraLeaf->cluster != cameraLeaf->cluster || !std::equal(areaMask, areaMask + MAX_MAP_AREA_BYTES, visCache->lastAreaMask))
		{
			// Clear data that will be recalculated.
			visCache->surfaces.clear();
			visCache->nSkies = 0;

			for (size_t i = 0; i < VisCache::maxSkies; i++)
			{
				visCache->skyMaterials[i] = nullptr;
			}

			visCache->portalSurfaces.clear();
			visCache->bounds.setupForAddingPoints();

			// A cluster of -1 means the camera is outside the PVS - draw everything.
			const uint8_t *pvs = cameraLeaf->cluster == -1 ? nullptr: &visData_[cameraLeaf->cluster * clusterBytes_];

			for (size_t i = firstLeaf_; i < nodes_.size(); i++)
			{
				auto &leaf = nodes_[i];

				// Check PVS.
				if (pvs && !(pvs[leaf.cluster >> 3] & (1 << (leaf.cluster & 7))))
					continue;

				// Check for door connection.
				if (pvs && (areaMask[leaf.area >> 3] & (1 << (leaf.area & 7))))
					continue;

				// Merge this leaf's bounds.
				visCache->bounds.addPoints(leaf.bounds);

				for (size_t j = 0; j < leaf.nSurfaces; j++)
				{
					const int si = leafSurfaces_[leaf.firstSurface + j];

					// Ignore surfaces in models.
					if (si < 0 || si >= surfaces_.size())
						continue;
					
					auto &surface = surfaces_[si];

					// Don't add duplicates.
					if (surface.duplicateId == duplicateSurfaceId_)
						continue;

					// Ignore flares.
					if (surface.type == SurfaceType::Ignore || surface.type == SurfaceType::Flare)
						continue;

					// Add the surface.
					surface.duplicateId = duplicateSurfaceId_;
					
					if (surface.material->isSky)
					{
						// Special case for sky surfaces.
						size_t k;

						for (k = 0; k < VisCache::maxSkies; k++)
						{
							if (visCache->skyMaterials[k] == nullptr)
							{
								visCache->skyVertices[k].clear();
								visCache->skyMaterials[k] = surface.material;
								visCache->nSkies++;
								break;
							}
								
							if (visCache->skyMaterials[k] == surface.material)
								break;
						}
						
						if (k == VisCache::maxSkies)
						{
							ri.Printf(PRINT_WARNING, "Too many skies\n");
						}
						else
						{
							appendSkySurfaceGeometry(visCacheId, k, surface);
						}

						continue;
					}
					else
					{
						if (surface.material->isPortal)
						{
							visCache->portalSurfaces.push_back(&surface);
						}

						visCache->surfaces.push_back(&surface);
					}
				}
			}

			// Sort visible surfaces.
			std::sort(visCache->surfaces.begin(), visCache->surfaces.end(), surfaceCompare);

			// Clear indices.
			for (size_t i = 0; i < currentGeometryBuffer_ + 1; i++)
			{
				visCache->indices[i].clear();
			}

			// Clear CPU deform geometry.
			visCache->cpuDeformIndices.clear();
			visCache->cpuDeformVertices.clear();

			// Create batched surfaces.
			visCache->batchedSurfaces.clear();
			size_t firstSurface = 0;

			for (size_t i = 0; i < visCache->surfaces.size(); i++)
			{
				auto surface = visCache->surfaces[i];
				const bool isLast = i == visCache->surfaces.size() - 1;
				auto nextSurface = isLast ? nullptr : visCache->surfaces[i + 1];

				// Create new batch on certain surface state changes.
				if (!nextSurface || nextSurface->material != surface->material || nextSurface->fogIndex != surface->fogIndex || nextSurface->bufferIndex != surface->bufferIndex)
				{
					BatchedSurface bs;
					bs.material = surface->material;
					bs.fogIndex = surface->fogIndex;

					if (bs.material->hasCpuDeforms())
					{
						// Grab the geometry for all surfaces in this batch.
						// It will be copied into a transient buffer and then deformed every render() call.
						bs.firstIndex = (uint32_t)visCache->cpuDeformIndices.size();
						bs.nIndices = 0;

						for (size_t j = firstSurface; j <= i; j++)
						{
							auto s = visCache->surfaces[j];

							// Making assumptions about the geometry. CPU deforms are only used by autosprite and autosprite2, which assume triangulated quads.
							if ((s->indices.size() % 6) != 0)
							{
								ri.Printf(PRINT_WARNING, "CPU deform geometry for material %s had odd index count %d\n", bs.material->name, (int)s->indices.size());
							}

							// Make room in destination.
							const size_t firstDestIndex = visCache->cpuDeformIndices.size();
							visCache->cpuDeformIndices.resize(visCache->cpuDeformIndices.size() + s->indices.size());
							const size_t firstDestVertex = visCache->cpuDeformVertices.size();
							visCache->cpuDeformVertices.resize(visCache->cpuDeformVertices.size() + s->indices.size() / 6 * 4);

							// Iterate triangulated quads.
							for (size_t quadIndex = 0; quadIndex < s->indices.size() / 6; quadIndex++)
							{
								const uint16_t *srcIndices = &s->indices[quadIndex * 6];
								const Vertex *srcVertices = &vertices_[surface->bufferIndex][0];

								// Vertices may not be contiguous. Grab the unique vertices for this quad.
								auto quadVertices = ExtractQuadCorners(vertices_[surface->bufferIndex].data(), srcIndices);

								// Need to remap indices.
								std::array<uint16_t, 6> quadIndices;

								for (size_t l = 0; l < quadIndices.size(); l++)
								{
									for (size_t m = 0; m < quadVertices.size(); m++)
									{
										if (quadVertices[m] == &srcVertices[srcIndices[l]])
											quadIndices[l] = uint16_t(m);
									}
								}

								// Got a quad. Append it.
								for (size_t l = 0; l < quadVertices.size(); l++)
								{
									visCache->cpuDeformVertices[firstDestVertex + quadIndex * 4 + l] = *quadVertices[l];
								}

								for (size_t l = 0; l < quadIndices.size(); l++)
								{
									visCache->cpuDeformIndices[firstDestIndex + quadIndex * 6 + l] = uint16_t(firstDestVertex + quadIndex * 4 + quadIndices[l]);
								}

								bs.nIndices += 6;
							}
						}
					}
					else
					{
						// Grab the indices for all surfaces in this batch.
						// They will be used directly by a dynamic index buffer.
						bs.bufferIndex = surface->bufferIndex;
						auto &indices = visCache->indices[bs.bufferIndex];
						bs.firstIndex = (uint32_t)indices.size();
						bs.nIndices = 0;
					
						for (size_t j = firstSurface; j <= i; j++)
						{
							auto s = visCache->surfaces[j];
							const size_t copyIndex = indices.size();
							indices.resize(indices.size() + s->indices.size());
							memcpy(&indices[copyIndex], &s->indices[0], s->indices.size() * sizeof(uint16_t));
							bs.nIndices += (uint32_t)s->indices.size();
						}
					}

					visCache->batchedSurfaces.push_back(bs);
					firstSurface = i + 1;
				}
			}

			// Update dynamic index buffers.
			for (size_t i = 0; i < currentGeometryBuffer_ + 1; i++)
			{
				auto &ib = visCache->indexBuffers[i];
				auto &indices = visCache->indices[i];

				if (indices.empty())
					continue;

				auto mem = bgfx::copy(indices.data(), uint32_t(indices.size() * sizeof(uint16_t)));

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
		}

		visCache->lastCameraLeaf = cameraLeaf;
		memcpy(visCache->lastAreaMask, areaMask, sizeof(visCache->lastAreaMask));
		duplicateSurfaceId_++;
	}

	void render(DrawCallList *drawCallList, uint8_t visCacheId)
	{
		assert(drawCallList);
		auto &visCache = visCaches_[visCacheId];

		// Copy the CPU deform geo to a transient buffer.
		bgfx::TransientVertexBuffer tvb;
		bgfx::TransientIndexBuffer tib;
		bool cpuDeformEnabled = true;

		if (!visCache->cpuDeformVertices.empty() && !visCache->cpuDeformIndices.empty())
		{
			if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, visCache->cpuDeformVertices.size(), &tib, visCache->cpuDeformIndices.size()))
			{
				WarnOnce(WarnOnceId::TransientBuffer);
				cpuDeformEnabled = false;
			}
			else
			{
				memcpy(tib.data, visCache->cpuDeformIndices.data(), visCache->cpuDeformIndices.size() * sizeof(uint16_t));
				memcpy(tvb.data, visCache->cpuDeformVertices.data(), visCache->cpuDeformVertices.size() * sizeof(Vertex));
			}
		}

		for (auto &surface : visCache->batchedSurfaces)
		{
			DrawCall dc;
			dc.fogIndex = surface.fogIndex;
			dc.material = surface.material;
			dc.ib.firstIndex = surface.firstIndex;
			dc.ib.nIndices = surface.nIndices;

			if (surface.material->hasCpuDeforms())
			{
				if (!cpuDeformEnabled)
					continue;

				dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
				dc.vb.transientHandle = tvb;
				dc.vb.nVertices = visCache->cpuDeformVertices.size();
				dc.ib.transientHandle = tib;
			}
			else
			{
				dc.vb.type = DrawCall::BufferType::Static;
				dc.vb.staticHandle = vertexBuffers_[surface.bufferIndex].handle;
				dc.vb.nVertices = (uint32_t)vertices_[surface.bufferIndex].size();
				dc.ib.type = DrawCall::BufferType::Dynamic;
				dc.ib.dynamicHandle = visCache->indexBuffers[surface.bufferIndex].handle;
			}

			drawCallList->push_back(dc);
		}
	}

private:
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
	int lightGridBounds_[3];

	struct MaterialDef
	{
		char name[MAX_QPATH];
		int surfaceFlags;
		int contentFlags;
	};

	std::vector<MaterialDef> materials_;
	std::vector<cplane_t> planes_;

	struct ModelDef
	{
		size_t firstSurface;
		size_t nSurfaces;
		Bounds bounds;
	};

	std::vector<ModelDef> modelDefs_;

	/// First model surfaces.
	std::vector<Surface> surfaces_;

	VertexBuffer vertexBuffers_[maxWorldGeometryBuffers];

	/// Vertex data populated at load time.
	std::vector<Vertex> vertices_[maxWorldGeometryBuffers];
	
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

	static bool surfaceCompare(const Surface *s1, const Surface *s2)
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

	void loadFromBspFile()
	{
		// Header
		auto header = (dheader_t *)fileData_;

		const int version = LittleLong(header->version);

		if (version != BSP_VERSION )
		{
			ri.Error(ERR_DROP, "%s has wrong version number (%i should be %i)", name_, version, BSP_VERSION);
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
			auto &l = header->lumps[i];
			l.fileofs = LittleLong(l.fileofs);
			l.filelen = LittleLong(l.filelen);

			if (lumpSizes[i] != 0 && (l.filelen % lumpSizes[i]))
				ri.Error(ERR_DROP, "%s: lump %d has bad size", name_, (int)i);
		}

		// Entities
		{
			auto &lump = header->lumps[LUMP_ENTITIES];
			char *p = (char *)(&fileData_[lump.fileofs]);

			// Store for reference by the cgame.
			entityString_.resize(lump.filelen + 1);
			strcpy(entityString_.data(), p);
			entityParsePoint_ = entityString_.data();

			char *token = COM_ParseExt(&p, qtrue);

			if (*token && *token == '{')
			{
				for (;;)
				{	
					// Parse key.
					token = COM_ParseExt(&p, qtrue);

					if (!*token || *token == '}')
						break;

					char keyname[MAX_TOKEN_CHARS];
					Q_strncpyz(keyname, token, sizeof(keyname));

					// Parse value.
					token = COM_ParseExt( &p, qtrue );

					if (!*token || *token == '}')
						break;

					char value[MAX_TOKEN_CHARS];
					Q_strncpyz(value, token, sizeof(value));

					// Check for a different light grid size.
					if (!Q_stricmp(keyname, "gridsize"))
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

		for (auto &p : planes_)
		{
			p.signbits = 0;
		
			for (size_t i = 0; i < 3; i++)
			{
				p.normal[i] = LittleFloat(filePlane->normal[i]);

				if (p.normal[i] < 0)
				{
					p.signbits |= 1 << i;
				}
			}

			p.dist = LittleFloat(filePlane->dist);
			p.type = PlaneTypeForNormal(p.normal);
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
			auto &f = fogs_[i];
			auto &ff = fileFogs[i];
			f.originalBrushNumber = LittleLong(ff.brushNum);
			
			if ((unsigned)f.originalBrushNumber >= nBrushes)
			{
				ri.Error(ERR_DROP, "fog brushNumber out of range");
			}

			auto &brush = fileBrushes[f.originalBrushNumber];
			const int firstSide = LittleLong(brush.firstSide);

			if ((unsigned)firstSide > nBrushSides - 6)
			{
				ri.Error(ERR_DROP, "fog brush side number out of range");
			}

			// Brushes are always sorted with the axial sides first.
			int sideNum = firstSide + 0;
			int planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
			f.bounds[0][0] = -planes_[planeNum].dist;

			sideNum = firstSide + 1;
			planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
			f.bounds[1][0] = planes_[planeNum].dist;

			sideNum = firstSide + 2;
			planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
			f.bounds[0][1] = -planes_[planeNum].dist;

			sideNum = firstSide + 3;
			planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
			f.bounds[1][1] = planes_[planeNum].dist;

			sideNum = firstSide + 4;
			planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
			f.bounds[0][2] = -planes_[planeNum].dist;

			sideNum = firstSide + 5;
			planeNum = LittleLong(fileBrushSides[sideNum].planeNum);
			f.bounds[1][2] = planes_[planeNum].dist;

			// Get information from the material for fog parameters.
			auto material = g_materialCache->findMaterial(ff.shader, MaterialLightmapId::None, true);
			f.parms = material->fogParms;
			f.colorInt = ColorBytes4(f.parms.color[0] * g_identityLight, f.parms.color[1] * g_identityLight,  f.parms.color[2] * g_identityLight, 1.0f);
			const float d = f.parms.depthForOpaque < 1 ? 1 : f.parms.depthForOpaque;
			f.tcScale = 1.0f / (d * 8);

			// Set the gradient vector.
			sideNum = LittleLong(ff.visibleSide);
			f.hasSurface = sideNum != -1;

			if (f.hasSurface)
			{
				planeNum = LittleLong(fileBrushSides[firstSide + sideNum].planeNum);
				f.surface = vec4(-vec3(planes_[planeNum].normal), -planes_[planeNum].dist);
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
				const int sr = (int)ceil(sqrtf(nLightmaps));
				lightmapAtlasSize_ = 1;

				while (lightmapAtlasSize_ < sr)
					lightmapAtlasSize_ *= 2;

				lightmapAtlasSize_ = std::min(1024, lightmapAtlasSize_ * lightmapSize_);
				nLightmapsPerAtlas_ = pow(lightmapAtlasSize_ / lightmapSize_, 2);
				lightmapAtlases_.resize(ceil(nLightmaps / (float)nLightmapsPerAtlas_));

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
						for (size_t y = 0; y < lightmapSize_; y++)
						{
							for (size_t x = 0; x < lightmapSize_; x++)
							{
								const size_t srcOffset = (x + y * lightmapSize_) * 3;
								const int nLightmapsPerDimension = lightmapAtlasSize_ / lightmapSize_;
								const int lightmapX = (nAtlasedLightmaps % nLightmapsPerAtlas_) % nLightmapsPerDimension;
								const int lightmapY = (nAtlasedLightmaps % nLightmapsPerAtlas_) / nLightmapsPerDimension;
								const size_t destOffset = ((lightmapX * lightmapSize_ + x) + (lightmapY * lightmapSize_ + y) * lightmapAtlasSize_) * image.nComponents;
								memcpy(&image.memory->data[destOffset], &srcData[srcOffset], 3);
								image.memory->data[destOffset + 3] = 0xff;
							}
						}

						nAtlasedLightmaps++;
						lightmapIndex++;
						srcData += srcDataSize;

						if (nAtlasedLightmaps >= nLightmapsPerAtlas_ || lightmapIndex >= nLightmaps)
							break;
					}

					lightmapAtlases_[i] = g_textureCache->createTexture(va("*lightmap%d", (int)i), image, TextureType::ColorAlpha, TextureFlags::ClampToEdge);
				}
			}
		}

		// Models
		auto fileModels = (const dmodel_t *)(fileData_ + header->lumps[LUMP_MODELS].fileofs);
		modelDefs_.resize(header->lumps[LUMP_MODELS].filelen / sizeof(*fileModels));

		for (size_t i = 0; i < modelDefs_.size(); i++)
		{
			auto &m = modelDefs_[i];
			auto &fm = fileModels[i];
			m.firstSurface = LittleLong(fm.firstSurface);
			m.nSurfaces = LittleLong(fm.numSurfaces);
			m.bounds[0] = vec3(LittleLong(fm.mins[0]), LittleLong(fm.mins[1]), LittleLong(fm.mins[2]));
			m.bounds[1] = vec3(LittleLong(fm.maxs[0]), LittleLong(fm.maxs[1]), LittleLong(fm.maxs[2]));
		}

		// Light grid. Models must be parsed first.
		{
			assert(modelDefs_.size() > 0);
			auto &lump = header->lumps[LUMP_LIGHTGRID];

			lightGridInverseSize_.x = 1.0f / lightGridSize_.x;
			lightGridInverseSize_.y = 1.0f / lightGridSize_.y;
			lightGridInverseSize_.z = 1.0f / lightGridSize_.z;

			for (size_t i = 0; i < 3; i++)
			{
				lightGridOrigin_[i] = lightGridSize_[i] * ceil(modelDefs_[0].bounds.min[i] / lightGridSize_[i]);
				const float max = lightGridSize_[i] * floor(modelDefs_[0].bounds.max[i] / lightGridSize_[i]);
				lightGridBounds_[i] = (max - lightGridOrigin_[i]) / lightGridSize_[i] + 1;
			}

			const int numGridPoints = lightGridBounds_[0] * lightGridBounds_[1] * lightGridBounds_[2];

			if (lump.filelen != numGridPoints * 8)
			{
				ri.Printf(PRINT_WARNING, "WARNING: light grid mismatch\n");
			}
			else
			{
				lightGridData_.resize(lump.filelen);
				Com_Memcpy(lightGridData_.data(), &fileData_[lump.fileofs], lump.filelen);
			}
		}

		// Materials
		auto fileMaterial = (const dshader_t *)(fileData_ + header->lumps[LUMP_SHADERS].fileofs);
		materials_.resize(header->lumps[LUMP_SHADERS].filelen / sizeof(*fileMaterial));

		for (auto &m : materials_)
		{
			Q_strncpyz(m.name, fileMaterial->shader, sizeof(m.name));
			m.surfaceFlags = LittleLong(fileMaterial->surfaceFlags );
			m.contentFlags = LittleLong(fileMaterial->contentFlags );
			fileMaterial++;
		}

		// Vertices
		std::vector<Vertex> vertices(header->lumps[LUMP_DRAWVERTS].filelen / sizeof(drawVert_t));
		auto fileDrawVerts = (const drawVert_t *)(fileData_ + header->lumps[LUMP_DRAWVERTS].fileofs);

		for (size_t i = 0; i < vertices.size(); i++)
		{
			auto &v = vertices[i];
			const auto &fv = fileDrawVerts[i];
			v.pos = vec3(LittleFloat(fv.xyz[0]), LittleFloat(fv.xyz[1]), LittleFloat(fv.xyz[2]));
			v.normal = vec3(LittleFloat(fv.normal[0]), LittleFloat(fv.normal[1]), LittleFloat(fv.normal[2]));
			v.texCoord = vec2(LittleFloat(fv.st[0]), LittleFloat(fv.st[1]));
			v.texCoord2 = vec2(LittleFloat(fv.lightmap[0]), LittleFloat(fv.lightmap[1]));

			v.color = vec4
			(
				fv.color[0] * g_overbrightFactor / 255.0f,
				fv.color[1] * g_overbrightFactor / 255.0f,
				fv.color[2] * g_overbrightFactor / 255.0f,
				fv.color[3] / 255.0f
			);
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
			auto &s = surfaces_[i];
			auto &fs = fileSurfaces[i];
			s.fogIndex = LittleLong(fs.fogNum); // -1 means no fog
			const int type = LittleLong(fs.surfaceType);
			int lightmapIndex = LittleLong(fs.lightmapNum);

			// Trisoup is always vertex lit.
			if (type == MST_TRIANGLE_SOUP)
			{
				lightmapIndex = MaterialLightmapId::Vertex;
			}

			s.material = findMaterial(LittleLong(fs.shaderNum), lightmapIndex);

			// We may have a nodraw surface, because they might still need to be around for movement clipping.
			if (s.material->surfaceFlags & SURF_NODRAW)
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
				s.cullinfo.type = CULLINFO_PLANE | CULLINFO_BOX;
				s.cullinfo.bounds.setupForAddingPoints();

				for (int i = 0; i < nVertices; i++)
				{
					s.cullinfo.bounds.addPoint(vertices[firstVertex + i].pos);
				}

				// take the plane information from the lightmap vector
				for (int i = 0 ; i < 3 ; i++ ) {
					s.cullinfo.plane.normal[i] = LittleFloat( fs.lightmapVecs[2][i] );
				}

				s.cullinfo.plane.dist = vec3::dotProduct(vertices[firstVertex].pos, s.cullinfo.plane.normal );
				SetPlaneSignbits( &s.cullinfo.plane );
				s.cullinfo.plane.type = PlaneTypeForNormal(s.cullinfo.plane.normal );
			}
			else if (type == MST_TRIANGLE_SOUP)
			{
				s.type = SurfaceType::Mesh;
				setSurfaceGeometry(&s, &vertices[LittleLong(fs.firstVert)], LittleLong(fs.numVerts), &indices[LittleLong(fs.firstIndex)], LittleLong(fs.numIndexes), lightmapIndex);
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
			const auto &md = modelDefs_[i];
			auto model = std::make_unique<WorldModel>((int)i, md.nSurfaces, md.bounds);

			for (size_t j = 0; j < md.nSurfaces; j++)
			{
				auto &fs = fileSurfaces[md.firstSurface + j];
				const int type = LittleLong(fs.surfaceType);
				const int lightmapIndex = LittleLong(fs.lightmapNum);
				auto material = findMaterial(LittleLong(fs.shaderNum), lightmapIndex);

				if (type == MST_PLANAR || type == MST_TRIANGLE_SOUP)
				{
					model->addSurface(j, material, &vertices[LittleLong(fs.firstVert)], LittleLong(fs.numVerts), &indices[LittleLong(fs.firstIndex)], LittleLong(fs.numIndexes), lightmapIndex % nLightmapsPerAtlas_, lightmapAtlasSize_ / lightmapSize_);
				}
				else if (type == MST_PATCH)
				{
					model->addPatchSurface(j, material, LittleLong(fs.patchWidth), LittleLong(fs.patchHeight), &vertices[LittleLong(fs.firstVert)], lightmapIndex % nLightmapsPerAtlas_, lightmapAtlasSize_ / lightmapSize_);
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
			auto &n = nodes_[i];
			auto &fn = fileNodes[i];

			n.leaf = false;
			n.bounds[0] = vec3(LittleLong(fn.mins[0]), LittleLong(fn.mins[1]), LittleLong(fn.mins[2]));
			n.bounds[1] = vec3(LittleLong(fn.maxs[0]), LittleLong(fn.maxs[1]), LittleLong(fn.maxs[2]));
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
			auto &l = nodes_[firstLeaf_ + i];
			auto &fl = fileLeaves[i];

			l.leaf = true;
			l.bounds[0] = vec3(LittleLong(fl.mins[0]), LittleLong(fl.mins[1]), LittleLong(fl.mins[2]));
			l.bounds[1] = vec3(LittleLong(fl.maxs[0]), LittleLong(fl.maxs[1]), LittleLong(fl.maxs[2]));
			l.cluster = LittleLong(fl.cluster);
			l.area = LittleLong(fl.area);

			if (l.cluster  >= nClusters_)
			{
				nClusters_ = l.cluster + 1;
			}

			l.firstSurface = LittleLong(fl.firstLeafSurface);
			l.nSurfaces = LittleLong(fl.numLeafSurfaces);
		}	

		// Visibility
		const auto &visLump = header->lumps[LUMP_VISIBILITY];

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

	void setSurfaceGeometry(Surface *surface, const Vertex *vertices, size_t nVertices, const uint16_t *indices, size_t nIndices, int lightmapIndex)
	{
		auto *bufferVertices = &vertices_[currentGeometryBuffer_];

		// Increment the current vertex buffer if the vertices won't fit.
		if (bufferVertices->size() + nVertices >= UINT16_MAX)
		{
			if (++currentGeometryBuffer_ == maxWorldGeometryBuffers)
				ri.Error(ERR_DROP, "Not enough world vertex buffers");

			bufferVertices = &vertices_[currentGeometryBuffer_];
		}

		// Append the vertices into the current vertex buffer.
		auto startVertex = (const uint16_t)bufferVertices->size();
		bufferVertices->resize(bufferVertices->size() + nVertices);
		memcpy(&(*bufferVertices)[startVertex], vertices, nVertices * sizeof(Vertex));

		for (size_t i = 0; i < nVertices; i++)
		{
			Vertex *v = &(*bufferVertices)[startVertex + i];
			v->texCoord2 = AtlasTexCoord(v->texCoord2, lightmapIndex % nLightmapsPerAtlas_, lightmapAtlasSize_ / lightmapSize_);
		}

		// The surface needs to know which vertex buffer to use.
		surface->bufferIndex = currentGeometryBuffer_;

		// Copy indices into the surface. Relative indices are made absolute.
		surface->indices.resize(nIndices);

		for (size_t i = 0; i < nIndices; i++)
		{
			surface->indices[i] = indices[i] + startVertex;
		}
	}

	void appendSkySurfaceGeometry(uint8_t visCacheId, size_t skyIndex, const Surface &surface)
	{
		auto &visCache = visCaches_[visCacheId];
		const size_t startVertex = visCache->skyVertices[skyIndex].size();
		visCache->skyVertices[skyIndex].resize(visCache->skyVertices[skyIndex].size() + surface.indices.size());

		for (size_t i = 0; i < surface.indices.size(); i++)
		{
			visCache->skyVertices[skyIndex][startVertex + i] = vertices_[surface.bufferIndex][surface.indices[i]];
		}
	}

	Material *findMaterial(int materialIndex, int lightmapIndex)
	{
		if (materialIndex < 0 || materialIndex >= materials_.size())
		{
			ri.Error(ERR_DROP, "%s: bad material index %i", name_, materialIndex);
		}

		if (lightmapIndex > 0)
		{
			lightmapIndex /= nLightmapsPerAtlas_;
		}

		auto material = g_materialCache->findMaterial(materials_[materialIndex].name, lightmapIndex, true);

		// If the material had errors, just use default material.
		if (!material)
			return g_materialCache->getDefaultMaterial();

		return material;
	}

	Node *leafFromPosition(vec3 pos)
	{
		auto node = &nodes_[0];

		for (;;)
		{
			if (node->leaf)
				break;

			const float d = vec3::dotProduct(pos, node->plane->normal) - node->plane->dist;
			node = d > 0 ? node->children[0] : node->children[1];
		}
	
		return node;
	}

	void boxSurfaces_recursive(Node *node, Bounds bounds, Surface **list, int listsize, int *listlength, vec3 dir)
	{
		// do the tail recursion in a loop
		while (!node->leaf)
		{
			int s = BoxOnPlaneSide(&bounds.min[0], &bounds.max[0], node->plane);

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
				boxSurfaces_recursive(node->children[0], bounds, list, listsize, listlength, dir);
				node = node->children[1];
			}
		}

		// add the individual surfaces
		for (int i = 0; i < node->nSurfaces; i++)
		{
			Surface *surface = &surfaces_[leafSurfaces_[node->firstSurface + i]];

			if (*listlength >= listsize)
				break;
			
			// check if the surface has NOIMPACT or NOMARKS set
			if ((surface->material->surfaceFlags & (SURF_NOIMPACT | SURF_NOMARKS)) || (surface->material->contentFlags & CONTENTS_FOG))
			{
				surface->decalDuplicateId = decalDuplicateSurfaceId_;
			}
			// extra check for surfaces to avoid list overflows
			else if (surface->type == SurfaceType::Face)
			{
				// the face plane should go through the box
				int s = BoxOnPlaneSide(&bounds.min[0], &bounds.max[0], &surface->cullinfo.plane);

				if (s == 1 || s == 2)
				{
					surface->decalDuplicateId = decalDuplicateSurfaceId_;
				}
				else if (vec3::dotProduct(surface->cullinfo.plane.normal, dir) > -0.5)
				{
					// don't add faces that make sharp angles with the projection direction
					surface->decalDuplicateId = decalDuplicateSurfaceId_;
				}
			}
			else if (surface->type != SurfaceType::Patch && surface->type != SurfaceType::Mesh)
			{
				surface->decalDuplicateId = decalDuplicateSurfaceId_;
			}

			// check the viewCount because the surface may have already been added if it spans multiple leafs
			if (surface->decalDuplicateId != decalDuplicateSurfaceId_)
			{
				surface->decalDuplicateId = decalDuplicateSurfaceId_;
				list[*listlength] = surface;
				(*listlength)++;
			}
		}
	}

	void addMarkFragments(int numClipPoints, vec3 clipPoints[2][MAX_VERTS_ON_POLY], int numPlanes, const vec3 *normals, const float *dists, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer, int *returnedPoints, int *returnedFragments)
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
};

static std::unique_ptr<World> s_world;

namespace world {

void Load(const char *name)
{
	s_world = std::make_unique<World>();
	s_world->load(name);
}

void Unload()
{
	s_world.reset(nullptr);
}

bool IsLoaded()
{
	return s_world.get() != nullptr;
}

const Texture *GetLightmap(size_t index)
{
	assert(IsLoaded());
	return s_world->getLightmap(index);
}

bool GetEntityToken(char *buffer, int size)
{
	assert(IsLoaded());
	return s_world->getEntityToken(buffer, size);
}

bool HasLightGrid()
{
	assert(IsLoaded());
	return s_world->hasLightGrid();
}

void SampleLightGrid(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir)
{
	assert(IsLoaded());
	s_world->sampleLightGrid(position, ambientLight, directedLight, lightDir);
}

bool InPvs(vec3 position1, vec3 position2)
{
	assert(IsLoaded());
	return s_world->inPvs(position1, position2);
}

int FindFogIndex(vec3 position, float radius)
{
	assert(IsLoaded());
	return s_world->findFogIndex(position, radius);
}

int FindFogIndex(const Bounds &bounds)
{
	assert(IsLoaded());
	return s_world->findFogIndex(bounds);
}

void CalculateFog(int fogIndex, const mat4 &modelMatrix, const mat4 &modelViewMatrix, vec3 cameraPosition, vec3 localViewPosition, const mat3 &cameraRotation, vec4 *fogColor, vec4 *fogDistance, vec4 *fogDepth, float *eyeT)
{
	assert(IsLoaded());
	s_world->calculateFog(fogIndex, modelMatrix, modelViewMatrix, cameraPosition, localViewPosition, cameraRotation, fogColor, fogDistance, fogDepth, eyeT);
}

int MarkFragments(int numPoints, const vec3 *points, vec3 projection, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)
{
	assert(IsLoaded());
	return s_world->markFragments(numPoints, points, projection, maxPoints, pointBuffer, maxFragments, fragmentBuffer);
}

Bounds GetBounds(uint8_t visCacheId)
{
	assert(IsLoaded());
	return s_world->getBounds(visCacheId);
}

size_t GetNumSkies(uint8_t visCacheId)
{
	assert(IsLoaded());
	return s_world->getNumSkies(visCacheId);
}

void GetSky(uint8_t visCacheId, size_t index, Material **material, const std::vector<Vertex> **vertices)
{
	assert(IsLoaded());
	s_world->getSky(visCacheId, index, material, vertices);
}

size_t GetNumPortalSurfaces(uint8_t visCacheId)
{
	assert(IsLoaded());
	return s_world->getNumPortalSurfaces(visCacheId);
}

bool CalculatePortalCamera(uint8_t visCacheId, size_t portalSurfaceIndex, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, const std::vector<Entity> &entities, vec3 *pvsPosition, Transform *portalCamera, bool *isMirror, vec4 *portalPlane)
{
	assert(IsLoaded());
	return s_world->calculatePortalCamera(visCacheId, portalSurfaceIndex, mainCameraPosition, mainCameraRotation, mvp, entities, pvsPosition, portalCamera, isMirror, portalPlane);
}

uint8_t CreateVisCache()
{
	assert(IsLoaded());
	return s_world->createVisCache();
}

void UpdateVisCache(uint8_t visCacheId, vec3 cameraPosition, const uint8_t *areaMask)
{
	assert(IsLoaded());
	s_world->updateVisCache(visCacheId, cameraPosition, areaMask);
}

void Render(DrawCallList *drawCallList, uint8_t visCacheId)
{
	assert(IsLoaded());
	s_world->render(drawCallList, visCacheId);
}

} // namespace world
} // namespace renderer
