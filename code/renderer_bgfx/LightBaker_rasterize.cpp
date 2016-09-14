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
#include "LightBaker.h"
#include "World.h"

namespace renderer {
namespace light_baker {

typedef int lm_bool;
#define LM_FALSE 0
#define LM_TRUE  1

#if defined(_MSC_VER) && !defined(__cplusplus) // TODO: specific versions only?
#define inline __inline
#endif

#if defined(_MSC_VER) && (_MSC_VER <= 1700)
static inline lm_bool lm_finite(float a) { return _finite(a); }
#else
static inline lm_bool lm_finite(float a) { return std::isfinite(a); }
#endif

static inline int      lm_mini      (int     a, int     b) { return a < b ? a : b; }
static inline int      lm_maxi      (int     a, int     b) { return a > b ? a : b; }
static inline int      lm_absi      (int     a           ) { return a < 0 ? -a : a; }
static inline float    lm_minf      (float   a, float   b) { return a < b ? a : b; }
static inline float    lm_maxf      (float   a, float   b) { return a > b ? a : b; }
static inline float    lm_absf      (float   a           ) { return a < 0.0f ? -a : a; }

typedef struct lm_ivec2 { int x, y; } lm_ivec2;
static inline lm_ivec2 lm_i2        (int     x, int     y) { lm_ivec2 v = { x, y }; return v; }

typedef struct lm_vec2 { float x, y; } lm_vec2;
static inline lm_vec2  lm_v2i       (int     x, int     y) { lm_vec2 v = { (float)x, (float)y }; return v; }
static inline lm_vec2  lm_v2        (float   x, float   y) { lm_vec2 v = { x, y }; return v; }
static inline lm_vec2  lm_negate2   (lm_vec2 a           ) { return lm_v2(-a.x, -a.y); }
static inline lm_vec2  lm_add2      (lm_vec2 a, lm_vec2 b) { return lm_v2(a.x + b.x, a.y + b.y); }
static inline lm_vec2  lm_sub2      (lm_vec2 a, lm_vec2 b) { return lm_v2(a.x - b.x, a.y - b.y); }
static inline lm_vec2  lm_mul2      (lm_vec2 a, lm_vec2 b) { return lm_v2(a.x * b.x, a.y * b.y); }
static inline lm_vec2  lm_scale2    (lm_vec2 a, float   b) { return lm_v2(a.x * b, a.y * b); }
static inline lm_vec2  lm_div2      (lm_vec2 a, float   b) { return lm_scale2(a, 1.0f / b); }
static inline lm_vec2  lm_min2      (lm_vec2 a, lm_vec2 b) { return lm_v2(lm_minf(a.x, b.x), lm_minf(a.y, b.y)); }
static inline lm_vec2  lm_max2      (lm_vec2 a, lm_vec2 b) { return lm_v2(lm_maxf(a.x, b.x), lm_maxf(a.y, b.y)); }
static inline lm_vec2  lm_floor2    (lm_vec2 a           ) { return lm_v2(floorf(a.x), floorf(a.y)); }
static inline lm_vec2  lm_ceil2     (lm_vec2 a           ) { return lm_v2(ceilf (a.x), ceilf (a.y)); }
static inline float    lm_dot2      (lm_vec2 a, lm_vec2 b) { return a.x * b.x + a.y * b.y; }
static inline float    lm_cross2    (lm_vec2 a, lm_vec2 b) { return a.x * b.y - a.y * b.x; } // pseudo cross product
static inline float    lm_length2sq (lm_vec2 a           ) { return a.x * a.x + a.y * a.y; }
static inline float    lm_length2   (lm_vec2 a           ) { return sqrtf(lm_length2sq(a)); }
static inline lm_vec2  lm_normalize2(lm_vec2 a           ) { return lm_div2(a, lm_length2(a)); }
static inline lm_bool  lm_finite2   (lm_vec2 a           ) { return lm_finite(a.x) && lm_finite(a.y); }

typedef struct lm_vec3 { float x, y, z; } lm_vec3;
static inline lm_vec3  lm_v3        (float   x, float   y, float   z) { lm_vec3 v = { x, y, z }; return v; }
static inline lm_vec3  lm_negate3   (lm_vec3 a           ) { return lm_v3(-a.x, -a.y, -a.z); }
static inline lm_vec3  lm_add3      (lm_vec3 a, lm_vec3 b) { return lm_v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline lm_vec3  lm_sub3      (lm_vec3 a, lm_vec3 b) { return lm_v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline lm_vec3  lm_mul3      (lm_vec3 a, lm_vec3 b) { return lm_v3(a.x * b.x, a.y * b.y, a.z * b.z); }
static inline lm_vec3  lm_scale3    (lm_vec3 a, float   b) { return lm_v3(a.x * b, a.y * b, a.z * b); }
static inline lm_vec3  lm_div3      (lm_vec3 a, float   b) { return lm_scale3(a, 1.0f / b); }
static inline lm_vec3  lm_min3      (lm_vec3 a, lm_vec3 b) { return lm_v3(lm_minf(a.x, b.x), lm_minf(a.y, b.y), lm_minf(a.z, b.z)); }
static inline lm_vec3  lm_max3      (lm_vec3 a, lm_vec3 b) { return lm_v3(lm_maxf(a.x, b.x), lm_maxf(a.y, b.y), lm_maxf(a.z, b.z)); }
static inline lm_vec3  lm_floor3    (lm_vec3 a           ) { return lm_v3(floorf(a.x), floorf(a.y), floorf(a.z)); }
static inline lm_vec3  lm_ceil3     (lm_vec3 a           ) { return lm_v3(ceilf (a.x), ceilf (a.y), ceilf (a.z)); }
static inline float    lm_dot3      (lm_vec3 a, lm_vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline lm_vec3  lm_cross3    (lm_vec3 a, lm_vec3 b) { return lm_v3(a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y); }
static inline float    lm_length3sq (lm_vec3 a           ) { return a.x * a.x + a.y * a.y + a.z * a.z; }
static inline float    lm_length3   (lm_vec3 a           ) { return sqrtf(lm_length3sq(a)); }
static inline lm_vec3  lm_normalize3(lm_vec3 a           ) { return lm_div3(a, lm_length3(a)); }
static inline lm_bool  lm_finite3   (lm_vec3 a           ) { return lm_finite(a.x) && lm_finite(a.y) && lm_finite(a.z); }

static lm_vec2 lm_toBarycentric(lm_vec2 p1, lm_vec2 p2, lm_vec2 p3, lm_vec2 p)
{
	// http://www.blackpawn.com/texts/pointinpoly/
	// Compute vectors
	lm_vec2 v0 = lm_sub2(p3, p1);
	lm_vec2 v1 = lm_sub2(p2, p1);
	lm_vec2 v2 = lm_sub2(p, p1);
	// Compute dot products
	float dot00 = lm_dot2(v0, v0);
	float dot01 = lm_dot2(v0, v1);
	float dot02 = lm_dot2(v0, v2);
	float dot11 = lm_dot2(v1, v1);
	float dot12 = lm_dot2(v1, v2);
	// Compute barycentric coordinates
	float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
	return lm_v2(u, v);
}

static inline int lm_leftOf(lm_vec2 a, lm_vec2 b, lm_vec2 c)
{
	float x = lm_cross2(lm_sub2(b, a), lm_sub2(c, b));
	return x < 0 ? -1 : x > 0;
}

static lm_bool lm_lineIntersection(lm_vec2 x0, lm_vec2 x1, lm_vec2 y0, lm_vec2 y1, lm_vec2* res)
{
	lm_vec2 dx = lm_sub2(x1, x0);
	lm_vec2 dy = lm_sub2(y1, y0);
	lm_vec2 d = lm_sub2(x0, y0);
	float dyx = lm_cross2(dy, dx);
	if (dyx == 0.0f)
		return LM_FALSE;
	dyx = lm_cross2(d, dx) / dyx;
	if (dyx <= 0 || dyx >= 1)
		return LM_FALSE;
	res->x = y0.x + dyx * dy.x;
	res->y = y0.y + dyx * dy.y;
	return LM_TRUE;
}

// this modifies the poly array! the poly array must be big enough to hold the result!
// res must be big enough to hold the result!
static int lm_convexClip(lm_vec2 *poly, int nPoly, const lm_vec2 *clip, int nClip, lm_vec2 *res)
{
	int nRes = nPoly;
	int dir = lm_leftOf(clip[0], clip[1], clip[2]);
	for (int i = 0, j = nClip - 1; i < nClip && nRes; j = i++)
	{
		if (i != 0)
			for (nPoly = 0; nPoly < nRes; nPoly++)
				poly[nPoly] = res[nPoly];
		nRes = 0;
		lm_vec2 v0 = poly[nPoly - 1];
		int side0 = lm_leftOf(clip[j], clip[i], v0);
		if (side0 != -dir)
			res[nRes++] = v0;
		for (int k = 0; k < nPoly; k++)
		{
			lm_vec2 v1 = poly[k], x;
			int side1 = lm_leftOf(clip[j], clip[i], v1);
			if (side0 + side1 == 0 && side0 && lm_lineIntersection(clip[j], clip[i], v0, v1, &x))
				res[nRes++] = x;
			if (k == nPoly - 1)
				break;
			if (side1 != -dir)
				res[nRes++] = v1;
			v0 = v1;
			side0 = side1;
		}
	}

	return nRes;
}

struct lm_context
{
	struct
	{
		lm_vec3 p[3];
		lm_vec2 uv[3];
	} triangle;

	struct
	{
		int minx, miny;
		int maxx, maxy;
		int x, y;
	} rasterizer;

	struct
	{
		lm_vec3 position;
		lm_vec3 direction;
		lm_vec3 up;
	} sample;
};

static lm_bool lm_hasConservativeTriangleRasterizerFinished(lm_context *ctx)
{
	return ctx->rasterizer.y >= ctx->rasterizer.maxy;
}

static void lm_moveToNextPotentialConservativeTriangleRasterizerPosition(lm_context *ctx)
{
	if (++ctx->rasterizer.x >= ctx->rasterizer.maxx)
	{
		ctx->rasterizer.x = ctx->rasterizer.minx;
		++ctx->rasterizer.y;
	}
}

static lm_bool lm_trySamplingConservativeTriangleRasterizerPosition(lm_context *ctx)
{
	if (lm_hasConservativeTriangleRasterizerFinished(ctx))
		return LM_FALSE;

	lm_vec2 pixel[16];
	pixel[0] = lm_v2i(ctx->rasterizer.x, ctx->rasterizer.y);
	pixel[1] = lm_v2i(ctx->rasterizer.x + 1, ctx->rasterizer.y);
	pixel[2] = lm_v2i(ctx->rasterizer.x + 1, ctx->rasterizer.y + 1);
	pixel[3] = lm_v2i(ctx->rasterizer.x, ctx->rasterizer.y + 1);

	lm_vec2 res[16];
	int nRes = lm_convexClip(pixel, 4, ctx->triangle.uv, 3, res);
	if (nRes > 0)
	{
		// do centroid sampling
		lm_vec2 centroid = res[0];
		float area = res[nRes - 1].x * res[0].y - res[nRes - 1].y * res[0].x;
		for (int i = 1; i < nRes; i++)
		{
			centroid = lm_add2(centroid, res[i]);
			area += res[i - 1].x * res[i].y - res[i - 1].y * res[i].x;
		}
		centroid = lm_div2(centroid, (float)nRes);
		area = lm_absf(area / 2.0f);

		if (area > 0.0f)
		{
			// calculate 3D sample position and orientation
			lm_vec2 uv = lm_toBarycentric(
				ctx->triangle.uv[0],
				ctx->triangle.uv[1],
				ctx->triangle.uv[2],
				centroid);

			// sample it only if its's not degenerate
			if (lm_finite2(uv))
			{
				lm_vec3 p0 = ctx->triangle.p[0];
				lm_vec3 p1 = ctx->triangle.p[1];
				lm_vec3 p2 = ctx->triangle.p[2];
				lm_vec3 v1 = lm_sub3(p1, p0);
				lm_vec3 v2 = lm_sub3(p2, p0);
				ctx->sample.position = lm_add3(p0, lm_add3(lm_scale3(v2, uv.x), lm_scale3(v1, uv.y)));
				ctx->sample.direction = lm_normalize3(lm_cross3(v1, v2));

				if (lm_finite3(ctx->sample.position) &&
					lm_finite3(ctx->sample.direction) &&
					lm_length3sq(ctx->sample.direction) > 0.5f) // don't allow 0.0f. should always be ~1.0f
				{
					// randomize rotation
					lm_vec3 up = lm_v3(0.0f, 0.0f, 1.0f);
					if (lm_absf(lm_dot3(up, ctx->sample.direction)) > 0.8f)
						up = lm_v3(1.0f, 0.0f, 0.0f);
					lm_vec3 side = lm_normalize3(lm_cross3(up, ctx->sample.direction));
					up = lm_normalize3(lm_cross3(side, ctx->sample.direction));
					int rx = ctx->rasterizer.x % 3;
					int ry = ctx->rasterizer.y % 3;
					const float pi = 3.14159265358979f; // no c++ M_PI?
					const float baseAngle = 0.03f * pi;
					const float baseAngles[3][3] = {
						{ baseAngle, baseAngle + 1.0f / 3.0f, baseAngle + 2.0f / 3.0f },
						{ baseAngle + 1.0f / 3.0f, baseAngle + 2.0f / 3.0f, baseAngle },
						{ baseAngle + 2.0f / 3.0f, baseAngle, baseAngle + 1.0f / 3.0f }
					};
					float phi = 2.0f * pi * baseAngles[ry][rx] + 0.1f * ((float)rand() / (float)RAND_MAX);
					ctx->sample.up = lm_normalize3(lm_add3(lm_scale3(side, cosf(phi)), lm_scale3(up, sinf(phi))));

					return LM_TRUE;
				}
			}
		}
	}
	return LM_FALSE;
}

// returns true if a sampling position was found and
// false if we finished rasterizing the current triangle
static lm_bool lm_findFirstConservativeTriangleRasterizerPosition(lm_context *ctx)
{
	while (!lm_trySamplingConservativeTriangleRasterizerPosition(ctx))
	{
		lm_moveToNextPotentialConservativeTriangleRasterizerPosition(ctx);
		if (lm_hasConservativeTriangleRasterizerFinished(ctx))
			return LM_FALSE;
	}
	return LM_TRUE;
}

static lm_bool lm_findNextConservativeTriangleRasterizerPosition(lm_context *ctx)
{
	lm_moveToNextPotentialConservativeTriangleRasterizerPosition(ctx);
	return lm_findFirstConservativeTriangleRasterizerPosition(ctx);
}

void RasterizeLightmappedSurfaces()
{
	int64_t startTime = bx::getHPCounter();
	s_lightBaker->totalLuxels = 0;
	const vec2i lightmapSize = world::GetLightmapSize();

	for (int mi = 0; mi < world::GetNumModels(); mi++)
	{
		for (int si = 0; si < world::GetNumSurfaces(mi); si++)
		{
			const world::Surface &surface = world::GetSurface(mi, si);

			// Ignore surfaces that aren't lightmapped.
			if (surface.type == world::SurfaceType::Ignore || surface.type == world::SurfaceType::Flare || surface.material->lightmapIndex < 0)
				continue;

			// Iterate surface triangles.
			const std::vector<Vertex> &vertices = world::GetVertexBuffer(surface.bufferIndex);

			for (size_t ti = 0; ti < surface.indices.size() / 3; ti++)
			{
				// Setup rasterizer.
				lm_context ctx;
				lm_vec2 uvMin = lm_v2(FLT_MAX, FLT_MAX), uvMax = lm_v2(-FLT_MAX, -FLT_MAX);

				for (int i = 0; i < 3; i++)
				{
					const Vertex &v = vertices[surface.indices[ti * 3 + i]];
					ctx.triangle.p[i].x = v.pos.x;
					ctx.triangle.p[i].y = v.pos.y;
					ctx.triangle.p[i].z = v.pos.z;
					ctx.triangle.uv[i].x = v.texCoord2.x * lightmapSize.x;
					ctx.triangle.uv[i].y = v.texCoord2.y * lightmapSize.y;

					// update bounds on lightmap
					uvMin = lm_min2(uvMin, ctx.triangle.uv[i]);
					uvMax = lm_max2(uvMax, ctx.triangle.uv[i]);
				}

				// Calculate area of interest (on lightmap) for conservative rasterization.
				lm_vec2 bbMin = lm_floor2(uvMin);
				lm_vec2 bbMax = lm_ceil2(uvMax);
				ctx.rasterizer.minx = ctx.rasterizer.x = lm_maxi((int)bbMin.x - 1, 0);
				ctx.rasterizer.miny = ctx.rasterizer.y = lm_maxi((int)bbMin.y - 1, 0);
				ctx.rasterizer.maxx = lm_mini((int)bbMax.x + 1, lightmapSize.x);
				ctx.rasterizer.maxy = lm_mini((int)bbMax.y + 1, lightmapSize.y);
				assert(ctx.rasterizer.minx < ctx.rasterizer.maxx && ctx.rasterizer.miny < ctx.rasterizer.maxy);

				// Rasterize.
				bool startedSampling = false;

				for (;;)
				{
					lm_bool gotSample;

					if (!startedSampling)
					{
						gotSample = lm_findFirstConservativeTriangleRasterizerPosition(&ctx);
						startedSampling = true;
					}
					else
					{
						gotSample = lm_findNextConservativeTriangleRasterizerPosition(&ctx);
					}

					if (!gotSample)
						break;

					Lightmap &lightmap = s_lightBaker->lightmaps[surface.material->lightmapIndex];
					Luxel luxel;
					luxel.position = vec3(&ctx.sample.position.x);
					luxel.normal = -vec3(&ctx.sample.direction.x);
					luxel.up = vec3(&ctx.sample.up.x);
					luxel.offset = ctx.rasterizer.x + ctx.rasterizer.y * world::GetLightmapSize().x;

					// Avoid adding luxels that occupy the same offset (different triangles rasterizing the same pixels).
					// Can use Lightmap::colorBytes, it will be overwritten later anyway.
					uint8_t &luxelExists = lightmap.colorBytes[luxel.offset * 4];

					if (luxelExists == 0)
					{
						luxelExists = 1;
						lightmap.luxels.push_back(luxel);
						s_lightBaker->totalLuxels++;
					}
				}
			}
		}
	}

	s_lightBaker->rasterizationTime = bx::getHPCounter() - startTime;
}

} // namespace light_baker
} // namespace renderer
