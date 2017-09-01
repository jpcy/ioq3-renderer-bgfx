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

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2
#define SKY_SUBDIVISIONS		8
#define HALF_SKY_SUBDIVISIONS	(SKY_SUBDIVISIONS/2)
#define SQR(a) ((a)*(a))
#define	ON_EPSILON		0.1f			// point on plane side epsilon
#define	MAX_CLIP_VERTS	64

namespace renderer {

static vec3 sky_clip[6] = 
{
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};

static float	sky_mins[2][6], sky_maxs[2][6];
static float	sky_min, sky_max;

static vec3	s_skyPoints[SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1];
static float	s_skyTexCoords[SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1][2];

static float s_cloudTexCoords[6][SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1][2];
static float s_cloudTexP[6][SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1];

static void AddSkyPolygon(int nump, vec3 *vecs) 
{
	int		i,j;
	vec3	av;
	float	s, t, dv;
	int		axis;
	// s = [0]/[2], t = [1]/[2]
	static int	vec_to_st[6][3] =
	{
		{-2,3,1},
		{2,3,-1},

		{1,3,2},
		{-1,3,-2},

		{-2,-1,3},
		{-2,1,-3}

	//	{-1,2,3},
	//	{1,2,-3}
	};

	// decide which face it maps to
	vec3 v;

	for (i=0; i<nump ; i++)
	{
		v += vecs[i];
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++)
	{
		const vec3 &v = vecs[i];

		j = vec_to_st[axis][2];
		if (j > 0)
			dv = v[j - 1];
		else
			dv = -v[-j - 1];
		if (dv < 0.001)
			continue;	// don't divide by zero
		j = vec_to_st[axis][0];
		if (j < 0)
			s = -v[-j -1] / dv;
		else
			s = v[j-1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -v[-j -1] / dv;
		else
			t = v[j-1] / dv;

		if (s < sky_mins[0][axis])
			sky_mins[0][axis] = s;
		if (t < sky_mins[1][axis])
			sky_mins[1][axis] = t;
		if (s > sky_maxs[0][axis])
			sky_maxs[0][axis] = s;
		if (t > sky_maxs[1][axis])
			sky_maxs[1][axis] = t;
	}
}

static void ClipSkyPolygon(int nump, vec3 *vecs, int stage) 
{
	bool front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		interface::Error("ClipSkyPolygon: MAX_CLIP_VERTS");
	if (stage == 6)
	{	// fully clipped, so draw it
		AddSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;

	for (i=0; i<nump ; i++)
	{
		d = vec3::dotProduct(vecs[i], sky_clip[stage]);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	vecs[i] = vecs[0];
	newc[0] = newc[1] = 0;

	for (i=0; i<nump ; i++)
	{
		const vec3 &v = vecs[i];

		switch (sides[i])
		{
		case SIDE_FRONT:
			newv[0][newc[0]] = v;
			newc[0]++;
			break;
		case SIDE_BACK:
			newv[1][newc[1]] = v;
			newc[1]++;
			break;
		case SIDE_ON:
			newv[0][newc[0]] = v;
			newc[0]++;
			newv[1][newc[1]] = v;
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(vecs[i + 1][j] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon(newc[0], &newv[0][0], stage+1);
	ClipSkyPolygon(newc[1], &newv[1][0], stage+1);
}

/*
** MakeSkyVec
**
** Parms: s, t range from -1 to 1
*/
static void MakeSkyVec(float zMax, float s, float t, int axis, float outSt[2], vec3 *outXYZ)
{
	// 1 = s, 2 = t, 3 = 2048
	static int	st_to_vec[6][3] =
	{
		{3,-1,2},
		{-3,1,2},

		{1,3,2},
		{-1,-3,2},

		{-2,-1,3},		// 0 degrees yaw, look straight up
		{2,-1,-3}		// look straight down
	};

	int			j, k;
	float	boxSize;

	boxSize = zMax / 1.75f;		// div sqrt(3)
	vec3 b;
	b[0] = s*boxSize;
	b[1] = t*boxSize;
	b[2] = boxSize;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
		{
			(*outXYZ)[j] = -b[-k - 1];
		}
		else
		{
			(*outXYZ)[j] = b[k - 1];
		}
	}

	// avoid bilerp seam
	s = (s+1)*0.5f;
	t = (t+1)*0.5f;
	if (s < sky_min)
	{
		s = sky_min;
	}
	else if (s > sky_max)
	{
		s = sky_max;
	}

	if (t < sky_min)
	{
		t = sky_min;
	}
	else if (t > sky_max)
	{
		t = sky_max;
	}

	t = 1.0f - t;


	if (outSt)
	{
		outSt[0] = s;
		outSt[1] = t;
	}
}

/// Either tessellate, or do a dry run to calculate the total number of vertices and indices to use.
static bool TessellateSkyBoxSide(int i, Vertex *vertices, uint16_t *indices, uint32_t *nVertices, uint32_t *nIndices, vec3 cameraPosition, float zMax)
{
	assert((vertices && indices) || (nVertices && nIndices));
	
	uint32_t currentVertex = 0, currentIndex = 0;

	sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
	sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
	sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
	sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;

	if (sky_mins[0][i] >= sky_maxs[0][i] || sky_mins[1][i] >= sky_maxs[1][i])
		return false;

	int sky_mins_subd[2], sky_maxs_subd[2];;
	sky_mins_subd[0] = int(sky_mins[0][i] * HALF_SKY_SUBDIVISIONS);
	sky_mins_subd[1] = int(sky_mins[1][i] * HALF_SKY_SUBDIVISIONS);
	sky_maxs_subd[0] = int(sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS);
	sky_maxs_subd[1] = int(sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS);

	if ( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
		sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
	else if ( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS ) 
		sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
	if ( sky_mins_subd[1] < -HALF_SKY_SUBDIVISIONS )
		sky_mins_subd[1] = -HALF_SKY_SUBDIVISIONS;
	else if ( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS ) 
		sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

	if ( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
		sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
	else if ( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS ) 
		sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
	if ( sky_maxs_subd[1] < -HALF_SKY_SUBDIVISIONS ) 
		sky_maxs_subd[1] = -HALF_SKY_SUBDIVISIONS;
	else if ( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS ) 
		sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

	//
	// iterate through the subdivisions
	//
	for (int t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++ )
	{
		for (int s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++ )
		{
			MakeSkyVec(zMax, ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
						( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
						i, 
						s_skyTexCoords[t][s], 
						&s_skyPoints[t][s]);
		}
	}

	const uint32_t startVertex = currentVertex;

	for (int t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++ )
	{
		for (int s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++ )
		{
			if (vertices)
			{
				vertices[currentVertex].pos = s_skyPoints[t][s] + cameraPosition;
				vertices[currentVertex].setTexCoord(s_skyTexCoords[t][s][0], s_skyTexCoords[t][s][1]);
			}

			currentVertex++;
		}
	}

	int tHeight = sky_maxs_subd[1] - sky_mins_subd[1] + 1;
	int sWidth = sky_maxs_subd[0] - sky_mins_subd[0] + 1;

	for (int t = 0; t < tHeight-1; t++ )
	{
		for (int s = 0; s < sWidth-1; s++ )
		{
			if (indices)
			{
				indices[currentIndex + 0] = startVertex + s + t * sWidth;
				indices[currentIndex + 1] = startVertex + s + (t + 1) * sWidth;
				indices[currentIndex + 2] = startVertex + s + 1 + t * sWidth;
				indices[currentIndex + 3] = startVertex + s + (t + 1) * sWidth;
				indices[currentIndex + 4] = startVertex + s + 1 + (t + 1) * sWidth;
				indices[currentIndex + 5] = startVertex + s + 1 + t * sWidth;
			}

			currentIndex += 6;
		}
	}

	if (nVertices && nIndices)
	{
		*nVertices = currentVertex;
		*nIndices = currentIndex;
	}

	return true;
}

/// Either tessellate, or do a dry run to calculate the total number of vertices and indices to use.
static void TessellateCloudBox(Vertex *vertices, uint16_t *indices, uint32_t *nVertices, uint32_t *nIndices, vec3 cameraPosition, float zMax)
{
	assert((vertices && indices) || (nVertices && nIndices));
	sky_min = 1.0 / 256.0f;		// FIXME: not correct?
	sky_max = 255.0 / 256.0f;
	uint32_t currentVertex = 0, currentIndex = 0;

	for (int sideIndex = 0; sideIndex < 6; sideIndex++)
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		float MIN_T;

		if (1) // FIXME? shader->sky.fullClouds)
		{
			MIN_T = -HALF_SKY_SUBDIVISIONS;

			// still don't want to draw the bottom, even if fullClouds
			if (sideIndex == 5)
				continue;
		}
		else
		{
			switch(sideIndex)
			{
			case 0:
			case 1:
			case 2:
			case 3:
				MIN_T = -1;
				break;
			case 5:
				// don't draw clouds beneath you
				continue;
			case 4:		// top
			default:
				MIN_T = -HALF_SKY_SUBDIVISIONS;
				break;
			}
		}

		sky_mins[0][sideIndex] = floor(sky_mins[0][sideIndex] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][sideIndex] = floor(sky_mins[1][sideIndex] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][sideIndex] = ceil(sky_maxs[0][sideIndex] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][sideIndex] = ceil(sky_maxs[1][sideIndex] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;

		if ((sky_mins[0][sideIndex] >= sky_maxs[0][sideIndex]) || (sky_mins[1][sideIndex] >= sky_maxs[1][sideIndex]))
			continue;

		sky_mins_subd[0] = std::lrintf(sky_mins[0][sideIndex] * HALF_SKY_SUBDIVISIONS);
		sky_mins_subd[1] = std::lrintf(sky_mins[1][sideIndex] * HALF_SKY_SUBDIVISIONS);
		sky_maxs_subd[0] = std::lrintf(sky_maxs[0][sideIndex] * HALF_SKY_SUBDIVISIONS);
		sky_maxs_subd[1] = std::lrintf(sky_maxs[1][sideIndex] * HALF_SKY_SUBDIVISIONS);

		if (sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS) 
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if (sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS) 
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		if (sky_mins_subd[1] < MIN_T)
			sky_mins_subd[1] = (int)MIN_T;
		else if (sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS) 
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

		if (sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS) 
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if (sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS) 
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		if (sky_maxs_subd[1] < MIN_T)
			sky_maxs_subd[1] = (int)MIN_T;
		else if (sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS) 
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

		// iterate through the subdivisions
		for (int t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++)
		{
			for (int s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++)
			{
				MakeSkyVec(zMax, (s - HALF_SKY_SUBDIVISIONS) / (float) HALF_SKY_SUBDIVISIONS, 
							(t - HALF_SKY_SUBDIVISIONS) / (float) HALF_SKY_SUBDIVISIONS, 
							sideIndex, 
							NULL,
							&s_skyPoints[t][s]);

				s_skyTexCoords[t][s][0] = s_cloudTexCoords[sideIndex][t][s][0];
				s_skyTexCoords[t][s][1] = s_cloudTexCoords[sideIndex][t][s][1];
			}
		}

		int tHeight = sky_maxs_subd[1] - sky_mins_subd[1] + 1;
		int sWidth = sky_maxs_subd[0] - sky_mins_subd[0] + 1;
		const uint32_t startVertex = currentVertex;

		for (int t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++)
		{
			for (int s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++)
			{
				if (vertices)
				{
					vertices[currentVertex].pos = s_skyPoints[t][s] + cameraPosition;
					vertices[currentVertex].setTexCoord(s_skyTexCoords[t][s][0], s_skyTexCoords[t][s][1]);
				}

				currentVertex++;
			}
		}

		for (int t = 0; t < tHeight-1; t++)
		{	
			for (int s = 0; s < sWidth-1; s++)
			{
				if (indices)
				{
					indices[currentIndex + 0] = startVertex + s + t * sWidth;
					indices[currentIndex + 1] = startVertex + s + (t + 1) * sWidth;
					indices[currentIndex + 2] = startVertex + s + 1 + t * sWidth;
					indices[currentIndex + 3] = startVertex + s + (t + 1) * sWidth;
					indices[currentIndex + 4] = startVertex + s + 1 + (t + 1) * sWidth;
					indices[currentIndex + 5] = startVertex + s + 1 + t * sWidth;
				}
				
				currentIndex += 6;
			}
		}
	}

	if (nVertices && nIndices)
	{
		*nVertices = currentVertex;
		*nIndices = currentIndex;
	}
}

void Sky_InitializeTexCoords(float heightCloud)
{
	int i, s, t;
	float radiusWorld = 4096;
	float p;
	float sRad, tRad;
	vec3 skyVec;

	for (i = 0; i < 6; i++)
	{
		for (t = 0; t <= SKY_SUBDIVISIONS; t++)
		{
			for (s = 0; s <= SKY_SUBDIVISIONS; s++)
			{
				// compute vector from view origin to sky side integral point
				MakeSkyVec(1024, (s - HALF_SKY_SUBDIVISIONS) / (float) HALF_SKY_SUBDIVISIONS, 
							(t - HALF_SKY_SUBDIVISIONS) / (float) HALF_SKY_SUBDIVISIONS, 
							i, 
							NULL,
							&skyVec);

				// compute parametric value 'p' that intersects with cloud layer
				p = (1.0f / (2 * vec3::dotProduct(skyVec, skyVec))) *
					(-2 * skyVec[2] * radiusWorld + 
					   2 * sqrt(SQR(skyVec[2]) * SQR(radiusWorld) + 
					             2 * SQR(skyVec[0]) * radiusWorld * heightCloud +
								 SQR(skyVec[0]) * SQR(heightCloud) + 
								 2 * SQR(skyVec[1]) * radiusWorld * heightCloud +
								 SQR(skyVec[1]) * SQR(heightCloud) + 
								 2 * SQR(skyVec[2]) * radiusWorld * heightCloud +
								 SQR(skyVec[2]) * SQR(heightCloud)));

				s_cloudTexP[i][t][s] = p;

				// compute intersection point based on p
				vec3 v = skyVec * p;
				v[2] += radiusWorld;

				// compute vector from world origin to intersection point 'v'
				v.normalize();

				sRad = ArcCos(v[0]);
				tRad = ArcCos(v[1]);

				s_cloudTexCoords[i][t][s][0] = sRad;
				s_cloudTexCoords[i][t][s][1] = tRad;
			}
		}
	}
}

void Sky_Render(DrawCallList *drawCallList, vec3 cameraPosition, float zMax, const SkySurface &surface)
{
	assert(drawCallList);

	const bool shouldDrawSkyBox = surface.material->sky.outerbox[0] && surface.material->sky.outerbox[0] != g_textureCache->getDefault();
	const bool shouldDrawCloudBox = surface.material->sky.cloudHeight > 0 && surface.material->stages[0].active;

	if (!shouldDrawSkyBox && !shouldDrawCloudBox)
		return;

	// Clear sky box.
	for (size_t i = 0; i < 6; i++)
	{
		sky_mins[0][i] = sky_mins[1][i] = 9999;
		sky_maxs[0][i] = sky_maxs[1][i] = -9999;
	}

	// Clip sky polygons.
	for (size_t i = 0; i < surface.vertices.size(); i += 3)
	{
		vec3 p[5]; // need one extra point for clipping

		for (size_t j = 0 ; j < 3 ; j++) 
		{
			p[j] = surface.vertices[i + j].pos - cameraPosition;
		}

		ClipSkyPolygon(3, p, 0);
	}

	// Draw the skybox.
	if (shouldDrawSkyBox)
	{
		for (int i = 0; i < 6; i++)
		{
			uint32_t nVertices, nIndices;
			sky_min = 0;
			sky_max = 1;
			memset( s_skyTexCoords, 0, sizeof( s_skyTexCoords ) );

			if (!TessellateSkyBoxSide(i, nullptr, nullptr, &nVertices, &nIndices, cameraPosition, zMax))
				continue;

			DrawCall dc;

			if (!bgfx::allocTransientBuffers(&dc.vb.transientHandle, Vertex::decl, nVertices, &dc.ib.transientHandle, nIndices)) 
			{
				WarnOnce(WarnOnceId::TransientBuffer);
				return;
			}

			sky_min = 0;
			sky_max = 1;
			memset( s_skyTexCoords, 0, sizeof( s_skyTexCoords ) );
			TessellateSkyBoxSide(i, (Vertex *)dc.vb.transientHandle.data, (uint16_t *)dc.ib.transientHandle.data, nullptr, nullptr, cameraPosition, zMax);
			dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
			dc.vb.nVertices = nVertices;
			dc.ib.nIndices = nIndices;
			dc.flags = DrawCallFlags::Sky | DrawCallFlags::Skybox;
			dc.material = surface.material;
			dc.skyboxSide = i;
			dc.state |= BGFX_STATE_DEPTH_TEST_LEQUAL;

			// Write depth as 1.
			dc.zOffset = 1.0f;
			dc.zScale = 0.0f;
			drawCallList->push_back(dc);
		}
	}

	// Draw the clouds.
	if (shouldDrawCloudBox)
	{
		uint32_t nVertices, nIndices;
		TessellateCloudBox(nullptr, nullptr, &nVertices, &nIndices, cameraPosition, zMax);
		DrawCall dc;

		if (!bgfx::allocTransientBuffers(&dc.vb.transientHandle, Vertex::decl, nVertices, &dc.ib.transientHandle, nIndices)) 
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			return;
		}

		TessellateCloudBox((Vertex *)dc.vb.transientHandle.data, (uint16_t *)dc.ib.transientHandle.data, nullptr, nullptr, cameraPosition, zMax);
		dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
		dc.vb.nVertices = nVertices;
		dc.ib.nIndices = nIndices;
		dc.flags = DrawCallFlags::Sky;
		dc.material = surface.material;
		dc.sort = 1; // Render after the skybox.

		// Write depth as 1.
		dc.zOffset = 1.0f;
		dc.zScale = 0.0f;
		drawCallList->push_back(dc);
	}
}

} // namespace renderer
