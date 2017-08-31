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

#define PATCH_STITCHING
#define SUBDIVISIONS 1 // r_subdivisions.getFloat()
#define	MAX_PATCH_SIZE		32			// max dimensions of a patch mesh in map file
#define	MAX_GRID_SIZE		65			// max dimensions of a grid mesh in memory

namespace renderer {

/*

This file does all of the processing necessary to turn a raw grid of points
read from the map file into a Patch ready for rendering.

The level of detail solution is direction independent, based only on subdivided
distance from the true curve.

Only a single entry point:

Patch *Patch_Subdivide( int width, int height,
								Vertex points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] ) {

*/


/*
============
LerpDrawVert
============
*/
static void LerpDrawVert( Vertex *a, Vertex *b, Vertex *out )
{
	out->pos = vec3::lerp(a->pos, b->pos, 0.5f);
	out->setTexCoord(vec4::lerp(a->getTexCoord(), b->getTexCoord(), 0.5f));
	out->color = vec4b::lerp(a->color, b->color, 0.5f);
}

/*
============
Transpose
============
*/
static void Transpose( int width, int height, Vertex ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE] ) {
	int		i, j;
	Vertex	temp;

	if ( width > height ) {
		for ( i = 0 ; i < height ; i++ ) {
			for ( j = i + 1 ; j < width ; j++ ) {
				if ( j < height ) {
					// swap the value
					temp = ctrl[j][i];
					ctrl[j][i] = ctrl[i][j];
					ctrl[i][j] = temp;
				} else {
					// just copy
					ctrl[j][i] = ctrl[i][j];
				}
			}
		}
	} else {
		for ( i = 0 ; i < width ; i++ ) {
			for ( j = i + 1 ; j < height ; j++ ) {
				if ( j < width ) {
					// swap the value
					temp = ctrl[i][j];
					ctrl[i][j] = ctrl[j][i];
					ctrl[j][i] = temp;
				} else {
					// just copy
					ctrl[i][j] = ctrl[j][i];
				}
			}
		}
	}

}


/*
=================
MakeMeshNormals

Handles all the complicated wrapping and degenerate cases
=================
*/
static void MakeMeshNormals( int width, int height, Vertex ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE] ) {
	int		i, j, k, dist;
	vec3	normal;
	vec3	sum;
	int		count = 0;
	vec3	base;
	vec3	delta;
	int		x, y;
	Vertex	*dv;
	vec3		around[8], temp;
	bool good[8];
	bool wrapWidth, wrapHeight;
	float		len;
static	int	neighbors[8][2] = {
	{0,1}, {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}
	};

	wrapWidth = false;
	for ( i = 0 ; i < height ; i++ ) {
		delta = ctrl[i][0].pos - ctrl[i][width-1].pos;
		len = delta.lengthSquared();
		if ( len > 1.0 ) {
			break;
		}
	}
	if ( i == height ) {
		wrapWidth = true;
	}

	wrapHeight = false;
	for ( i = 0 ; i < width ; i++ ) {
		delta = ctrl[0][i].pos - ctrl[height-1][i].pos;
		len = delta.lengthSquared();
		if ( len > 1.0 ) {
			break;
		}
	}
	if ( i == width) {
		wrapHeight = true;
	}


	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 0 ; j < height ; j++ ) {
			count = 0;
			dv = &ctrl[j][i];
			base = dv->pos;
			for ( k = 0 ; k < 8 ; k++ ) {
				around[k] = vec3::empty;
				good[k] = false;

				for ( dist = 1 ; dist <= 3 ; dist++ ) {
					x = i + neighbors[k][0] * dist;
					y = j + neighbors[k][1] * dist;
					if ( wrapWidth ) {
						if ( x < 0 ) {
							x = width - 1 + x;
						} else if ( x >= width ) {
							x = 1 + x - width;
						}
					}
					if ( wrapHeight ) {
						if ( y < 0 ) {
							y = height - 1 + y;
						} else if ( y >= height ) {
							y = 1 + y - height;
						}
					}

					if ( x < 0 || x >= width || y < 0 || y >= height ) {
						break;					// edge of patch
					}
					temp = ctrl[y][x].pos - base;
					if (temp.normalize() == 0 ) {
						continue;				// degenerate edge, get more dist
					} else {
						good[k] = true;
						around[k] = temp;
						break;					// good edge
					}
				}
			}

			sum = vec3::empty;
			for ( k = 0 ; k < 8 ; k++ ) {
				if ( !good[k] || !good[(k+1)&7] ) {
					continue;	// didn't get two points
				}
				normal = vec3::crossProduct(around[(k+1)&7], around[k]);
				if (normal.normalize() == 0 ) {
					continue;
				}
				sum += normal;
				count++;
			}
			//if ( count == 0 ) {
			//	printf("bad normal\n");
			//}
			//VectorNormalize2( sum, dv->normal );
			vec3 normal = vec3(sum);
			normal.normalizeFast();
			dv->setNormal(normal);
		}
	}
}

static int MakeMeshIndexes(int width, int height, Vertex ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE],
							 uint16_t indexes[(MAX_GRID_SIZE-1)*(MAX_GRID_SIZE-1)*2*3])
{
	int             i, j;
	int             numIndexes;
	int             w, h;
	Vertex      *dv;
	static Vertex       ctrl2[MAX_GRID_SIZE * MAX_GRID_SIZE];

	h = height - 1;
	w = width - 1;
	numIndexes = 0;
	for(i = 0; i < h; i++)
	{
		for(j = 0; j < w; j++)
		{
			int             v1, v2, v3, v4;

			// vertex order to be reckognized as tristrips
			v1 = i * width + j + 1;
			v2 = v1 - 1;
			v3 = v2 + width;
			v4 = v3 + 1;

			indexes[numIndexes++] = v2;
			indexes[numIndexes++] = v3;
			indexes[numIndexes++] = v1;

			indexes[numIndexes++] = v1;
			indexes[numIndexes++] = v3;
			indexes[numIndexes++] = v4;
		}
	}

	// FIXME: use more elegant way
	for(i = 0; i < width; i++)
	{
		for(j = 0; j < height; j++)
		{
			dv = &ctrl2[j * width + i];
			*dv = ctrl[j][i];
		}
	}

	return numIndexes;
}


/*
============
InvertCtrl
============
*/
static void InvertCtrl( int width, int height, Vertex ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE] ) {
	int		i, j;
	Vertex	temp;

	for ( i = 0 ; i < height ; i++ ) {
		for ( j = 0 ; j < width/2 ; j++ ) {
			temp = ctrl[i][j];
			ctrl[i][j] = ctrl[i][width-1-j];
			ctrl[i][width-1-j] = temp;
		}
	}
}


/*
=================
InvertErrorTable
=================
*/
static void InvertErrorTable( float errorTable[2][MAX_GRID_SIZE], int width, int height ) {
	int		i;
	float	copy[2][MAX_GRID_SIZE];

	memcpy( copy, errorTable, sizeof( copy ) );

	for ( i = 0 ; i < width ; i++ ) {
		errorTable[1][i] = copy[0][i];	//[width-1-i];
	}

	for ( i = 0 ; i < height ; i++ ) {
		errorTable[0][i] = copy[1][height-1-i];
	}

}

/*
==================
PutPointsOnCurve
==================
*/
static void PutPointsOnCurve( Vertex	ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE], 
							 int width, int height ) {
	int			i, j;
	Vertex	prev, next;

	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 1 ; j < height ; j += 2 ) {
			LerpDrawVert( &ctrl[j][i], &ctrl[j+1][i], &prev );
			LerpDrawVert( &ctrl[j][i], &ctrl[j-1][i], &next );
			LerpDrawVert( &prev, &next, &ctrl[j][i] );
		}
	}


	for ( j = 0 ; j < height ; j++ ) {
		for ( i = 1 ; i < width ; i += 2 ) {
			LerpDrawVert( &ctrl[j][i], &ctrl[j][i+1], &prev );
			LerpDrawVert( &ctrl[j][i], &ctrl[j][i-1], &next );
			LerpDrawVert( &prev, &next, &ctrl[j][i] );
		}
	}
}

/*
=================
R_CreateSurfaceGridMesh
=================
*/
Patch *R_CreateSurfaceGridMesh(int width, int height,
								Vertex ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE], float errorTable[2][MAX_GRID_SIZE],
								int numIndexes, uint16_t indexes[(MAX_GRID_SIZE-1)*(MAX_GRID_SIZE-1)*2*3]) {
	int i, j, size;
	Vertex	*vert;
	Patch *grid;

	// copy the results out to a grid
	size = (width * height - 1) * sizeof( Vertex ) + sizeof( *grid );

#ifdef PATCH_STITCHING
	grid = /*ri.Hunk_Alloc*/ (Patch *)malloc(size);
	memset(grid, 0, size);

	grid->widthLodError = /*ri.Hunk_Alloc*/ (float *)malloc(width * 4 );
	memcpy( grid->widthLodError, errorTable[0], width * 4 );

	grid->heightLodError = /*ri.Hunk_Alloc*/ (float *)malloc(height * 4 );
	memcpy( grid->heightLodError, errorTable[1], height * 4 );

	grid->numIndexes = numIndexes;
	grid->indexes = (uint16_t *)malloc(grid->numIndexes * sizeof(uint16_t));
	memcpy(grid->indexes, indexes, numIndexes * sizeof(uint16_t));

	grid->numVerts = (width * height);
	grid->verts = (Vertex *)malloc(grid->numVerts * sizeof(Vertex));
#else
	grid = ri.Hunk_Alloc( size );
	memset(grid, 0, size);

	grid->widthLodError = ri.Hunk_Alloc( width * 4 );
	memcpy( grid->widthLodError, errorTable[0], width * 4 );

	grid->heightLodError = ri.Hunk_Alloc( height * 4 );
	memcpy( grid->heightLodError, errorTable[1], height * 4 );

	grid->numIndexes = numIndexes;
	grid->indexes = ri.Hunk_Alloc(grid->numIndexes * sizeof(uint16_t), h_low);
	memcpy(grid->indexes, indexes, numIndexes * sizeof(uint16_t));

	grid->numVerts = (width * height);
	grid->verts = ri.Hunk_Alloc(grid->numVerts * sizeof(Vertex), h_low);
#endif

	grid->width = width;
	grid->height = height;

	grid->cullBounds.setupForAddingPoints();

	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 0 ; j < height ; j++ ) {
			vert = &grid->verts[j*width+i];
			*vert = ctrl[j][i];
			grid->cullBounds.addPoint(vert->pos);
		}
	}

	// compute local origin and bounds
	grid->cullOrigin = grid->cullBounds.midpoint();
	grid->cullRadius = (grid->cullBounds.min - grid->cullOrigin).length();
	grid->lodOrigin = grid->cullOrigin;
	grid->lodRadius = grid->cullRadius;
	return grid;
}

/*
=================
Patch_Free
=================
*/
void Patch_Free( Patch *grid ) {
	free(grid->widthLodError);
	free(grid->heightLodError);
	free(grid->indexes);
	free(grid->verts);
	free(grid);
}

/*
=================
Patch_Subdivide
=================
*/
Patch *Patch_Subdivide( int width, int height, const Vertex *points) {
	int			i, j, k, l;
	Vertex prev;
	Vertex next;
	Vertex mid;
	float		len, maxLen;
	int			dir;
	int			t;
	Vertex	ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE];
	float		errorTable[2][MAX_GRID_SIZE];
	int			numIndexes;
	static uint16_t indexes[(MAX_GRID_SIZE-1)*(MAX_GRID_SIZE-1)*2*3];
	int consecutiveComplete;

	for ( i = 0 ; i < width ; i++ ) {
		for ( j = 0 ; j < height ; j++ ) {
			ctrl[j][i] = points[j*width+i];
		}
	}

	for ( dir = 0 ; dir < 2 ; dir++ ) {

		for ( j = 0 ; j < MAX_GRID_SIZE ; j++ ) {
			errorTable[dir][j] = 0;
		}

		consecutiveComplete = 0;

		// horizontal subdivisions
		for ( j = 0 ; ; j = (j + 2) % (width - 1) ) {
			// check subdivided midpoints against control points

			// FIXME: also check midpoints of adjacent patches against the control points
			// this would basically stitch all patches in the same LOD group together.

			maxLen = 0;
			for ( i = 0 ; i < height ; i++ ) {
				// calculate the point on the curve
				vec3 midxyz;

				for ( l = 0 ; l < 3 ; l++ ) {
					midxyz[l] = (ctrl[i][j].pos[l] + ctrl[i][j+1].pos[l] * 2
							+ ctrl[i][j+2].pos[l] ) * 0.25f;
				}

				// see how far off the line it is
				// using dist-from-line will not account for internal
				// texture warping, but it gives a lot less polygons than
				// dist-from-midpoint
				midxyz -= ctrl[i][j].pos;
				vec3 dir = ctrl[i][j+2].pos - ctrl[i][j].pos;
				dir.normalize();

				const float d = vec3::dotProduct(midxyz, dir);
				vec3 projected = dir * d;
				vec3 midxyz2 = midxyz - projected;
				len = midxyz2.lengthSquared(); // we will do the sqrt later
				if ( len > maxLen ) {
					maxLen = len;
				}
			}

			maxLen = sqrt(maxLen);

			// if all the points are on the lines, remove the entire columns
			if ( maxLen < 0.1f ) {
				errorTable[dir][j+1] = 999;
				// if we go over the whole grid twice without adding any columns, stop
				if (++consecutiveComplete >= width)
					break;
				continue;
			}

			// see if we want to insert subdivided columns
			if ( width + 2 > MAX_GRID_SIZE ) {
				errorTable[dir][j+1] = 1.0f/maxLen;
				break;	// can't subdivide any more
			}

			if ( maxLen <= SUBDIVISIONS ) {
				errorTable[dir][j+1] = 1.0f/maxLen;
				// if we go over the whole grid twice without adding any columns, stop
				if (++consecutiveComplete >= width)
					break;
				continue;	// didn't need subdivision
			}

			errorTable[dir][j+2] = 1.0f/maxLen;

			consecutiveComplete = 0;

			// insert two columns and replace the peak
			width += 2;
			for ( i = 0 ; i < height ; i++ ) {
				LerpDrawVert( &ctrl[i][j], &ctrl[i][j+1], &prev );
				LerpDrawVert( &ctrl[i][j+1], &ctrl[i][j+2], &next );
				LerpDrawVert( &prev, &next, &mid );

				for ( k = width - 1 ; k > j + 3 ; k-- ) {
					ctrl[i][k] = ctrl[i][k-2];
				}
				ctrl[i][j + 1] = prev;
				ctrl[i][j + 2] = mid;
				ctrl[i][j + 3] = next;
			}

			// skip the new one, we'll get it on the next pass
			j += 2;
		}

		Transpose( width, height, ctrl );
		t = width;
		width = height;
		height = t;
	}


	// put all the aproximating points on the curve
	PutPointsOnCurve( ctrl, width, height );

	// cull out any rows or columns that are colinear
	for ( i = 1 ; i < width-1 ; i++ ) {
		if ( errorTable[0][i] != 999 ) {
			continue;
		}
		for ( j = i+1 ; j < width ; j++ ) {
			for ( k = 0 ; k < height ; k++ ) {
				ctrl[k][j-1] = ctrl[k][j];
			}
			errorTable[0][j-1] = errorTable[0][j];
		}
		width--;
	}

	for ( i = 1 ; i < height-1 ; i++ ) {
		if ( errorTable[1][i] != 999 ) {
			continue;
		}
		for ( j = i+1 ; j < height ; j++ ) {
			for ( k = 0 ; k < width ; k++ ) {
				ctrl[j-1][k] = ctrl[j][k];
			}
			errorTable[1][j-1] = errorTable[1][j];
		}
		height--;
	}

#if 1
	// flip for longest tristrips as an optimization
	// the results should be visually identical with or
	// without this step
	if ( height > width ) {
		Transpose( width, height, ctrl );
		InvertErrorTable( errorTable, width, height );
		t = width;
		width = height;
		height = t;
		InvertCtrl( width, height, ctrl );
	}
#endif

	// calculate indexes
	numIndexes = MakeMeshIndexes(width, height, ctrl, indexes);

	// calculate normals
	MakeMeshNormals( width, height, ctrl );

	return R_CreateSurfaceGridMesh(width, height, ctrl, errorTable, numIndexes, indexes);
}

} // namespace renderer
