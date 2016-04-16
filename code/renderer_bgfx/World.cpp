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

static vec2 AtlasTexCoord(vec2 uv, int index, int nTilesPerDimension)
{
	const int tileX = index % nTilesPerDimension;
	const int tileY = index / nTilesPerDimension;
	vec2 result;
	result.u = (tileX / (float)nTilesPerDimension) + (uv.u / (float)nTilesPerDimension);
	result.v = (tileY / (float)nTilesPerDimension) + (uv.v / (float)nTilesPerDimension);
	return result;
}

WorldModel::WorldModel(int index, size_t nSurfaces, Bounds bounds) : tempSurfaces_(nSurfaces), bounds_(bounds)
{
	util::Sprintf(name_, sizeof(name_), "*%d", index);
}

Material *WorldModel::getMaterial(size_t surfaceNo) const
{
	// if it's out of range, return the first surface
	if (surfaceNo >= (int)surfaces_.size())
		surfaceNo = 0;

	const Surface &surface = surfaces_[surfaceNo];

	if (surface.material->lightmapIndex > MaterialLightmapId::None)
	{
		bool mipRawImage = true;

		// Get mipmap info for original texture.
		Texture *texture = Texture::get(surface.material->name);

		if (texture)
			mipRawImage = (texture->getFlags() & TextureFlags::Mipmap) != 0;

		Material *mat = g_materialCache->findMaterial(surface.material->name, MaterialLightmapId::None, mipRawImage);
		mat->stages[0].rgbGen = MaterialColorGen::LightingDiffuse; // (SA) new
		return mat;
	}

	return surface.material;
}

bool WorldModel::isCulled(Entity *entity, const Frustum &cameraFrustum) const
{
	return cameraFrustum.clipBounds(bounds_, mat4::transform(entity->rotation, entity->position)) == Frustum::ClipResult::Outside;
}

void WorldModel::render(const mat3 &scenRotation, DrawCallList *drawCallList, Entity *entity)
{
	assert(drawCallList);
	assert(entity);
	const mat4 modelMatrix = mat4::transform(entity->rotation, entity->position);

	for (Surface &surface : surfaces_)
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

void WorldModel::addPatchSurface(size_t index, Material *material, int width, int height, const Vertex *points, int lightmapIndex, int nLightmapTilesPerDimension)
{
	Patch *patch = Patch_Subdivide(width, height, points);
	addSurface(index, material, patch->verts, patch->numVerts, patch->indexes, patch->numIndexes, lightmapIndex, nLightmapTilesPerDimension);
	Patch_Free(patch);
}

void WorldModel::addSurface(size_t index, Material *material, const Vertex *vertices, size_t nVertices, const uint16_t *indices, size_t nIndices, int lightmapIndex, int nLightmapTilesPerDimension)
{
	// Create a temp surface.
	TempSurface &ts = tempSurfaces_[index];
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

		if (lightmapIndex >= 0)
			v->texCoord2 = AtlasTexCoord(v->texCoord2, lightmapIndex, nLightmapTilesPerDimension);
	}

	tempIndices_.resize(tempIndices_.size() + nIndices);
	memcpy(&tempIndices_[ts.firstIndex], indices, nIndices * sizeof(*indices));
}

void WorldModel::batchSurfaces()
{
	if (tempVertices_.empty() || tempIndices_.empty())
		return;

	// Allocate buffers for the batched geometry.
	const bgfx::Memory *verticesMem = bgfx::alloc(uint32_t(sizeof(Vertex) * tempVertices_.size()));
	auto vertices = (Vertex *)verticesMem->data;
	const bgfx::Memory *indicesMem = bgfx::alloc(uint32_t(sizeof(uint16_t) * tempIndices_.size()));
	auto indices = (uint16_t *)indicesMem->data;
	uint32_t currentVertex = 0, currentIndex = 0;

	for (;;)
	{
		Material *material = nullptr;

		// Get the material from the first temp surface that hasn't been batched.
		for (const TempSurface &ts : tempSurfaces_)
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

		for (Surface &s : surfaces_)
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
		for (TempSurface &ts : tempSurfaces_)
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

const Texture *World::getLightmap(size_t index) const
{
	return index < lightmapAtlases_.size() ? lightmapAtlases_[index] : nullptr;
}

bool World::getEntityToken(char *buffer, int size)
{
	const char *s = util::Parse(&entityParsePoint_, true);
	util::Strncpyz(buffer, s, size);

	if (!entityParsePoint_ && !s[0])
	{
		entityParsePoint_ = entityString_.data();
		return false;
	}

	return true;
}

bool World::hasLightGrid() const
{
	return !lightGridData_.empty();
}

void World::sampleLightGrid(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir) const
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
		pos[i] = (int)floor(v),
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

bool World::inPvs(vec3 position1, vec3 position2)
{
	Node *leaf = leafFromPosition(position1);
	const uint8_t *vis = interface::CM_ClusterPVS(leaf->cluster);
	leaf = leafFromPosition(position2);
	return ((vis[leaf->cluster >> 3] & (1 << (leaf->cluster & 7))) != 0);
}

int World::findFogIndex(vec3 position, float radius) const
{
	for (int i = 0; i < (int)fogs_.size(); i++)
	{
		const Fog &fog = fogs_[i];
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

int World::findFogIndex(const Bounds &bounds) const
{
	for (int i = 0; i < (int)fogs_.size(); i++)
	{
		if (Bounds::intersect(bounds, fogs_[i].bounds))
			return i;
	}

	return -1;
}

void World::calculateFog(int fogIndex, const mat4 &modelMatrix, const mat4 &modelViewMatrix, vec3 cameraPosition, vec3 localViewPosition, const mat3 &cameraRotation, vec4 *fogColor, vec4 *fogDistance, vec4 *fogDepth, float *eyeT) const
{
	assert(fogIndex != -1);
	assert(fogDistance);
	assert(fogDepth);
	assert(eyeT);
	const Fog &fog = fogs_[fogIndex];

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

int World::markFragments(int numPoints, const vec3 *points, const vec3 projection, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)
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
					Vertex *dv = surface->patch->verts + m * surface->patch->width + n;
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

			for (k = 0, tri = &surface->indices[0]; k < (int)surface->indices.size(); k += 3, tri += 3)
			{
				for (j = 0; j < 3; j++)
				{
					clipPoints[0][j] = vertices_[surface->bufferIndex][tri[j]].pos + surface->cullinfo.plane.normal * MARKER_OFFSET;
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

			for (k = 0, tri = &surface->indices[0]; k < (int)surface->indices.size(); k += 3, tri += 3)
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

Bounds World::getBounds() const
{
	return modelDefs_[0].bounds;
}

Bounds World::getBounds(uint8_t visCacheId) const
{
	return visCaches_[visCacheId]->bounds;
}

size_t World::getNumSkies(uint8_t visCacheId) const
{
	return visCaches_[visCacheId]->nSkies;
}

void World::getSky(uint8_t visCacheId, size_t index, Material **material, const std::vector<Vertex> **vertices) const
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

bool World::calculatePortalCamera(uint8_t visCacheId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, const std::vector<Entity> &entities, vec3 *pvsPosition, Transform *portalCamera, bool *isMirror, Plane *portalPlane) const
{
	assert(pvsPosition);
	assert(portalCamera);
	assert(isMirror);
	assert(portalPlane);
	const std::unique_ptr<VisCache> &visCache = visCaches_[visCacheId];

	// Calculate which portal surfaces in the PVS are visible to the camera.
	visCache->cameraPortalSurfaces.clear();

	for (Surface *portalSurface : visCache->portalSurfaces)
	{
		// Trivially reject.
		if (util::IsGeometryOffscreen(mvp, portalSurface->indices.data(), portalSurface->indices.size(), vertices_[portalSurface->bufferIndex].data()))
			continue;

		// Determine if this surface is backfaced and also determine the distance to the nearest vertex so we can cull based on portal range.
		// Culling based on vertex distance isn't 100% correct (we should be checking for range to the surface), but it's good enough for the types of portals we have in the game right now.
		float shortest;

		if (util::IsGeometryBackfacing(mainCameraPosition, portalSurface->indices.data(), portalSurface->indices.size(), vertices_[portalSurface->bufferIndex].data(), &shortest))
			continue;

		// Calculate surface plane.
		Plane plane;

		if (portalSurface->indices.size() >= 3)
		{
			const vec3 v1(vertices_[portalSurface->bufferIndex][portalSurface->indices[0]].pos);
			const vec3 v2(vertices_[portalSurface->bufferIndex][portalSurface->indices[1]].pos);
			const vec3 v3(vertices_[portalSurface->bufferIndex][portalSurface->indices[2]].pos);
			plane.normal = vec3::crossProduct(v3 - v1, v2 - v1).normal();
			plane.distance = vec3::dotProduct(v1, plane.normal);
		}
		else
		{
			plane.normal[0] = 1;
		}

		// Locate the portal entity closest to this plane.
		// Origin will be the origin of the portal, oldorigin will be the origin of the camera.
		const Entity *portalEntity = nullptr;

		for (size_t j = 0; j < entities.size(); j++)
		{
			const Entity &entity = entities[j];

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
		VisCache::Portal portal;
		portal.entity = portalEntity;
		portal.isMirror = isPortalMirror;
		portal.plane = plane;
		portal.surface = portalSurface;
		visCache->cameraPortalSurfaces.push_back(portal);
	}

	if (visCache->cameraPortalSurfaces.empty())
		return false;

	// All visible portal surfaces are required for writing to the stencil buffer, but we only need the first one to figure out the transform.
	const VisCache::Portal &portal = visCache->cameraPortalSurfaces[0];

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

bool World::calculateReflectionCamera(uint8_t visCacheId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, Transform *camera, Plane *plane)
{
	assert(camera);
	assert(plane);
	const std::unique_ptr<VisCache> &visCache = visCaches_[visCacheId];

	// Calculate which reflective surfaces in the PVS are visible to the camera.
	visCache->cameraReflectiveSurfaces.clear();

	for (Surface *surface : visCache->reflectiveSurfaces)
	{
		// Trivially reject.
		if (util::IsGeometryOffscreen(mvp, surface->indices.data(), surface->indices.size(), vertices_[surface->bufferIndex].data()))
			continue;

		// Determine if this surface is backfaced.
		if (util::IsGeometryBackfacing(mainCameraPosition, surface->indices.data(), surface->indices.size(), vertices_[surface->bufferIndex].data()))
			continue;

		// Reflective surface is visible to the camera.
		VisCache::Reflective reflective;
		reflective.surface = surface;

		if (surface->indices.size() >= 3)
		{
			const vec3 v1(vertices_[surface->bufferIndex][surface->indices[0]].pos);
			const vec3 v2(vertices_[surface->bufferIndex][surface->indices[1]].pos);
			const vec3 v3(vertices_[surface->bufferIndex][surface->indices[2]].pos);
			reflective.plane.normal = vec3::crossProduct(v3 - v1, v2 - v1).normal();
			reflective.plane.distance = vec3::dotProduct(v1, reflective.plane.normal);
		}
		else
		{
			reflective.plane.normal[0] = 1;
		}

		visCache->cameraReflectiveSurfaces.push_back(reflective);
	}

	if (visCache->cameraReflectiveSurfaces.empty())
		return false;

	// All visible reflective surfaces are required for writing to the stencil buffer, but we only need the first one to figure out the transform.
	const VisCache::Reflective &reflective = visCache->cameraReflectiveSurfaces[0];
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

void World::renderPortal(uint8_t visCacheId, DrawCallList *drawCallList)
{
	assert(drawCallList);
	std::unique_ptr<VisCache> &visCache = visCaches_[visCacheId];

	for (const VisCache::Portal &portal : visCache->cameraPortalSurfaces)
	{
		bgfx::TransientIndexBuffer tib;

		if (!bgfx::checkAvailTransientIndexBuffer((uint32_t)portal.surface->indices.size()))
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			return;
		}

		bgfx::allocTransientIndexBuffer(&tib, (uint32_t)portal.surface->indices.size());
		memcpy(tib.data, portal.surface->indices.data(), (uint32_t)portal.surface->indices.size() * sizeof(uint16_t));

		DrawCall dc;
		dc.material = portal.surface->material;
		dc.vb.type = DrawCall::BufferType::Static;
		dc.vb.staticHandle = vertexBuffers_[portal.surface->bufferIndex].handle;
		dc.vb.nVertices = (uint32_t)vertices_[portal.surface->bufferIndex].size();
		dc.ib.type = DrawCall::BufferType::Transient;
		dc.ib.transientHandle = tib;
		dc.ib.nIndices = (uint32_t)portal.surface->indices.size();
		drawCallList->push_back(dc);
	}
}

void World::renderReflective(uint8_t visCacheId, DrawCallList *drawCallList)
{
	assert(drawCallList);
	std::unique_ptr<VisCache> &visCache = visCaches_[visCacheId];

	for (const VisCache::Reflective &reflective : visCache->cameraReflectiveSurfaces)
	{
		bgfx::TransientIndexBuffer tib;

		if (!bgfx::checkAvailTransientIndexBuffer((uint32_t)reflective.surface->indices.size()))
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			return;
		}

		bgfx::allocTransientIndexBuffer(&tib, (uint32_t)reflective.surface->indices.size());
		memcpy(tib.data, reflective.surface->indices.data(), (uint32_t)reflective.surface->indices.size() * sizeof(uint16_t));

		DrawCall dc;
		dc.material = reflective.surface->material->reflectiveFrontSideMaterial;
		assert(dc.material);
		dc.vb.type = DrawCall::BufferType::Static;
		dc.vb.staticHandle = vertexBuffers_[reflective.surface->bufferIndex].handle;
		dc.vb.nVertices = (uint32_t)vertices_[reflective.surface->bufferIndex].size();
		dc.ib.type = DrawCall::BufferType::Transient;
		dc.ib.transientHandle = tib;
		dc.ib.nIndices = (uint32_t)reflective.surface->indices.size();
		drawCallList->push_back(dc);
	}
}

uint8_t World::createVisCache()
{
	visCaches_.push_back(std::make_unique<VisCache>());
	return uint8_t(visCaches_.size() - 1);
}

void World::updateVisCache(uint8_t visCacheId, vec3 cameraPosition, const uint8_t *areaMask)
{
	assert(areaMask);
	std::unique_ptr<VisCache> &visCache = visCaches_[visCacheId];

	// Get the PVS for the camera leaf cluster.
	Node *cameraLeaf = leafFromPosition(cameraPosition);

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
		visCache->reflectiveSurfaces.clear();
		visCache->bounds.setupForAddingPoints();

		// A cluster of -1 means the camera is outside the PVS - draw everything.
		const uint8_t *pvs = cameraLeaf->cluster == -1 ? nullptr: &visData_[cameraLeaf->cluster * clusterBytes_];

		for (size_t i = firstLeaf_; i < nodes_.size(); i++)
		{
			Node &leaf = nodes_[i];

			// Check PVS.
			if (pvs && !(pvs[leaf.cluster >> 3] & (1 << (leaf.cluster & 7))))
				continue;

			// Check for door connection.
			if (pvs && (areaMask[leaf.area >> 3] & (1 << (leaf.area & 7))))
				continue;

			// Merge this leaf's bounds.
			visCache->bounds.addPoints(leaf.bounds);

			for (int j = 0; j < leaf.nSurfaces; j++)
			{
				const int si = leafSurfaces_[leaf.firstSurface + j];

				// Ignore surfaces in models.
				if (si < 0 || si >= (int)surfaces_.size())
					continue;
					
				Surface &surface = surfaces_[si];

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
						interface::PrintWarningf("Too many skies\n");
					}
					else
					{
						appendSkySurfaceGeometry(visCacheId, k, surface);
					}

					continue;
				}
				else
				{
					if (surface.material->reflective == MaterialReflective::BackSide)
					{
						visCache->reflectiveSurfaces.push_back(&surface);
					}

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
			Surface *surface = visCache->surfaces[i];
			const bool isLast = i == visCache->surfaces.size() - 1;
			Surface *nextSurface = isLast ? nullptr : visCache->surfaces[i + 1];

			// Create new batch on certain surface state changes.
			if (!nextSurface || nextSurface->material != surface->material || nextSurface->fogIndex != surface->fogIndex || nextSurface->bufferIndex != surface->bufferIndex)
			{
				BatchedSurface bs;
				bs.contentFlags = surface->contentFlags;
				bs.fogIndex = surface->fogIndex;
				bs.material = surface->material;
				bs.surfaceFlags = surface->flags;

				if (bs.material->hasAutoSpriteDeform())
				{
					// Grab the geometry for all surfaces in this batch.
					// It will be copied into a transient buffer and then deformed every render() call.
					bs.firstIndex = (uint32_t)visCache->cpuDeformIndices.size();
					bs.nIndices = 0;
					bs.firstVertex = (uint32_t)visCache->cpuDeformVertices.size();
					bs.nVertices = 0;

					for (size_t j = firstSurface; j <= i; j++)
					{
						Surface *s = visCache->surfaces[j];

						// Make room in destination.
						const size_t firstDestIndex = visCache->cpuDeformIndices.size();
						visCache->cpuDeformIndices.resize(visCache->cpuDeformIndices.size() + s->indices.size());
						const size_t firstDestVertex = visCache->cpuDeformVertices.size();
						visCache->cpuDeformVertices.resize(visCache->cpuDeformVertices.size() + s->nVertices);

						// Append geometry.
						memcpy(&visCache->cpuDeformVertices[firstDestVertex], &vertices_[surface->bufferIndex][s->firstVertex], sizeof(Vertex) * s->nVertices);

						for (size_t k = 0; k < s->indices.size(); k++)
						{
							// Make indices relative.
							visCache->cpuDeformIndices[firstDestIndex + k] = uint16_t(s->indices[k] - s->firstVertex + bs.nVertices);
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
					std::vector<uint16_t> &indices = visCache->indices[bs.bufferIndex];
					bs.firstIndex = (uint32_t)indices.size();
					bs.nIndices = 0;

					for (size_t j = firstSurface; j <= i; j++)
					{
						Surface *s = visCache->surfaces[j];
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
			DynamicIndexBuffer &ib = visCache->indexBuffers[i];
			std::vector<uint16_t> &indices = visCache->indices[i];

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
	}

	visCache->lastCameraLeaf = cameraLeaf;
	memcpy(visCache->lastAreaMask, areaMask, sizeof(visCache->lastAreaMask));
	duplicateSurfaceId_++;
}

void World::render(uint8_t visCacheId, DrawCallList *drawCallList, const mat3 &sceneRotation)
{
	assert(drawCallList);
	std::unique_ptr<VisCache> &visCache = visCaches_[visCacheId];

	for (const BatchedSurface &surface : visCache->batchedSurfaces)
	{
		DrawCall dc;
		dc.flags = 0;

		if (surface.surfaceFlags & SURF_SKY)
			dc.flags |= DrawCallFlags::Sky;

		dc.fogIndex = surface.fogIndex;
		dc.material = surface.material;

		if (g_cvars.waterReflections.getBool())
		{
			// If this is a back side reflective material, use the front side material if there's any reflective surfaces visible to the camera.
			if (dc.material->reflective == MaterialReflective::BackSide && !visCache->cameraReflectiveSurfaces.empty())
			{
				dc.material = dc.material->reflectiveFrontSideMaterial;
			}
		}

		if (surface.material->hasAutoSpriteDeform())
		{
			assert(!visCache->cpuDeformVertices.empty() && !visCache->cpuDeformIndices.empty());
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
				
			memcpy(tib.data, &visCache->cpuDeformIndices[surface.firstIndex], surface.nIndices * sizeof(uint16_t));
			memcpy(tvb.data, &visCache->cpuDeformVertices[surface.firstVertex], surface.nVertices * sizeof(Vertex));
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
			dc.vb.staticHandle = vertexBuffers_[surface.bufferIndex].handle;
			dc.vb.nVertices = (uint32_t)vertices_[surface.bufferIndex].size();
			dc.ib.type = DrawCall::BufferType::Dynamic;
			dc.ib.dynamicHandle = visCache->indexBuffers[surface.bufferIndex].handle;
			dc.ib.firstIndex = surface.firstIndex;
			dc.ib.nIndices = surface.nIndices;
		}

		drawCallList->push_back(dc);
	}
}

bool World::surfaceCompare(const Surface *s1, const Surface *s2)
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

void World::overbrightenColor(const uint8_t *in, uint8_t *out)
{
	assert(in);
	assert(out);

	// Shift the data based on overbright range.
	int r = in[0] * g_overbrightFactor;
	int g = in[1] * g_overbrightFactor;
	int b = in[2] * g_overbrightFactor;

	// Normalize by color instead of saturating to white.
	if ((r | g | b) > 255)
	{
		int max = r > g ? r : g;
		max = max > b ? max : b;
		r = r * 255 / max;
		g = g * 255 / max;
		b = b * 255 / max;
	}

	out[0] = r;
	out[1] = g;
	out[2] = b;
}

void World::setSurfaceGeometry(Surface *surface, const Vertex *vertices, int nVertices, const uint16_t *indices, size_t nIndices, int lightmapIndex)
{
	std::vector<Vertex> *bufferVertices = &vertices_[currentGeometryBuffer_];

	// Increment the current vertex buffer if the vertices won't fit.
	if (bufferVertices->size() + nVertices >= UINT16_MAX)
	{
		if (++currentGeometryBuffer_ == maxWorldGeometryBuffers_)
			interface::Error("Not enough world vertex buffers");

		bufferVertices = &vertices_[currentGeometryBuffer_];
	}

	// Append the vertices into the current vertex buffer.
	auto startVertex = (const uint32_t)bufferVertices->size();
	bufferVertices->resize(bufferVertices->size() + nVertices);
	memcpy(&(*bufferVertices)[startVertex], vertices, nVertices * sizeof(Vertex));

	for (int i = 0; i < nVertices; i++)
	{
		Vertex *v = &(*bufferVertices)[startVertex + i];

		if (lightmapIndex >= 0 && !lightmapAtlases_.empty())
		{
			v->texCoord2 = AtlasTexCoord(v->texCoord2, lightmapIndex % nLightmapsPerAtlas_, lightmapAtlasSize_ / lightmapSize_);
		}
	}

	// The surface needs to know which vertex buffer to use.
	surface->bufferIndex = currentGeometryBuffer_;

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

void World::appendSkySurfaceGeometry(uint8_t visCacheId, size_t skyIndex, const Surface &surface)
{
	std::unique_ptr<VisCache> &visCache = visCaches_[visCacheId];
	const size_t startVertex = visCache->skyVertices[skyIndex].size();
	visCache->skyVertices[skyIndex].resize(visCache->skyVertices[skyIndex].size() + surface.indices.size());

	for (size_t i = 0; i < surface.indices.size(); i++)
	{
		visCache->skyVertices[skyIndex][startVertex + i] = vertices_[surface.bufferIndex][surface.indices[i]];
	}
}

Material *World::findMaterial(int materialIndex, int lightmapIndex)
{
	if (materialIndex < 0 || materialIndex >= (int)materials_.size())
	{
		interface::Error("%s: bad material index %i", name_, materialIndex);
	}

	if (lightmapIndex > 0)
	{
		lightmapIndex /= nLightmapsPerAtlas_;
	}

	Material *material = g_materialCache->findMaterial(materials_[materialIndex].name, lightmapIndex, true);

	// If the material had errors, just use default material.
	if (!material)
		return g_materialCache->getDefaultMaterial();

	return material;
}

World::Node *World::leafFromPosition(vec3 pos)
{
	Node *node = &nodes_[0];

	for (;;)
	{
		if (node->leaf)
			break;

		const float d = vec3::dotProduct(pos, node->plane->normal) - node->plane->distance;
		node = d > 0 ? node->children[0] : node->children[1];
	}
	
	return node;
}

void World::boxSurfaces_recursive(Node *node, Bounds bounds, Surface **list, int listsize, int *listlength, vec3 dir)
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
			int s = surface->cullinfo.plane.testBounds(bounds);

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

void World::addMarkFragments(int numClipPoints, vec3 clipPoints[2][MAX_VERTS_ON_POLY], int numPlanes, const vec3 *normals, const float *dists, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer, int *returnedPoints, int *returnedFragments)
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

namespace world {

static std::unique_ptr<World> s_world;

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

Bounds GetBounds()
{
	assert(IsLoaded());
	return s_world->getBounds();
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

bool CalculatePortalCamera(uint8_t visCacheId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, const std::vector<Entity> &entities, vec3 *pvsPosition, Transform *portalCamera, bool *isMirror, Plane *portalPlane)
{
	assert(IsLoaded());
	return s_world->calculatePortalCamera(visCacheId, mainCameraPosition, mainCameraRotation, mvp, entities, pvsPosition, portalCamera, isMirror, portalPlane);
}

bool CalculateReflectionCamera(uint8_t visCacheId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, Transform *camera, Plane *plane)
{
	assert(IsLoaded());
	return s_world->calculateReflectionCamera(visCacheId, mainCameraPosition, mainCameraRotation, mvp, camera, plane);
}

void RenderPortal(uint8_t visCacheId, DrawCallList *drawCallList)
{
	assert(IsLoaded());
	return s_world->renderPortal(visCacheId, drawCallList);
}

void RenderReflective(uint8_t visCacheId, DrawCallList *drawCallList)
{
	assert(IsLoaded());
	return s_world->renderReflective(visCacheId, drawCallList);
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

void Render(uint8_t visCacheId, DrawCallList *drawCallList, const mat3 &sceneRotation)
{
	assert(IsLoaded());
	s_world->render(visCacheId, drawCallList, sceneRotation);
}

} // namespace world
} // namespace renderer
