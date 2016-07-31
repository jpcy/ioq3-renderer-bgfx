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
/***********************************************************
* A single header file OpenGL lightmapping library         *
* https://github.com/ands/lightmapper                      *
* no warranty implied | use at your own risk               *
* author: Andreas Mantler (ands) | last change: 12.06.2016 *
*                                                          *
* License:                                                 *
* This software is in the public domain.                   *
* Where that dedication is not recognized,                 *
* you are granted a perpetual, irrevocable license to copy *
* and modify this file however you want.                   *
***********************************************************/
#include "Precompiled.h"
#pragma hdrstop

#if defined(USE_LIGHT_BAKER)

#undef Status // unknown source. affects linux build.

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
					lm_vec3 up = lm_v3(0.0f, 1.0f, 0.0f);
					if (lm_absf(lm_dot3(up, ctx->sample.direction)) > 0.8f)
						up = lm_v3(0.0f, 0.0f, 1.0f);
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

void lmImageDilate(const float *image, float *outImage, int w, int h, int c)
{
	assert(c > 0 && c <= 4);
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			float color[4];
			lm_bool valid = LM_FALSE;
			for (int i = 0; i < c; i++)
			{
				color[i] = image[(y * w + x) * c + i];
				valid |= color[i] > 0.0f;
			}
			if (!valid)
			{
				int n = 0;
				const int dx[] = { -1, 0, 1,  0 };
				const int dy[] = { 0, 1, 0, -1 };
				for (int d = 0; d < 4; d++)
				{
					int cx = x + dx[d];
					int cy = y + dy[d];
					if (cx >= 0 && cx < w && cy >= 0 && cy < h)
					{
						float dcolor[4];
						lm_bool dvalid = LM_FALSE;
						for (int i = 0; i < c; i++)
						{
							dcolor[i] = image[(cy * w + cx) * c + i];
							dvalid |= dcolor[i] > 0.0f;
						}
						if (dvalid)
						{
							for (int i = 0; i < c; i++)
								color[i] += dcolor[i];
							n++;
						}
					}
				}
				if (n)
				{
					float in = 1.0f / n;
					for (int i = 0; i < c; i++)
						color[i] *= in;
				}
			}
			for (int i = 0; i < c; i++)
				outImage[(y * w + x) * c + i] = color[i];
		}
	}
}

#define USE_LIGHT_BAKER_THREAD

struct LightBaker
{
	struct Lightmap
	{
		std::vector<vec4> color;
		std::vector<uint8_t> colorBytes;
	};

	enum class Status
	{
		NotStarted,
		Running,
		WaitingForTextureUpload,
		Finished,
		Cancelled,
		Error
	};

#ifdef USE_LIGHT_BAKER_THREAD
	SDL_mutex *mutex = nullptr;
	SDL_Thread *thread;
#endif

	int nSamples;
	static const int maxSamples = 16;
	int textureUploadFrameNo; // lightmap textures were uploaded this frame
	int nLuxelsProcessed;
	std::vector<Lightmap> lightmaps;
	static const size_t maxErrorMessageLen = 2048;
	char errorMessage[maxErrorMessageLen];
	RTCDevice embreeDevice = nullptr;
	RTCScene embreeScene = nullptr;

	// mutex protected state
	Status status;
	int progress = 0;

	~LightBaker()
	{
#ifdef USE_LIGHT_BAKER_THREAD
		if (mutex)
			SDL_DestroyMutex(mutex);
#endif
		if (embreeScene)
			rtcDeleteScene(embreeScene);

		if (embreeDevice)
			rtcDeleteDevice(embreeDevice);
	}
};

static std::unique_ptr<LightBaker> s_lightBaker;

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

static LightBaker::Status GetStatus(int *progress = nullptr)
{
	auto status = LightBaker::Status::NotStarted;

#ifdef USE_LIGHT_BAKER_THREAD
	if (SDL_LockMutex(s_lightBaker->mutex) == 0)
	{
#endif
		status = s_lightBaker->status;

		if (progress)
			*progress = s_lightBaker->progress;

#ifdef USE_LIGHT_BAKER_THREAD
		SDL_UnlockMutex(s_lightBaker->mutex);
	}
	else
	{
		if (progress)
			*progress = 0;
	}
#endif

	return status;
}

static void SetStatus(LightBaker::Status status, int progress = 0)
{
#ifdef USE_LIGHT_BAKER_THREAD
	if (SDL_LockMutex(s_lightBaker->mutex) == 0)
	{
#endif
		s_lightBaker->status = status;
		s_lightBaker->progress = progress;
#ifdef USE_LIGHT_BAKER_THREAD
		SDL_UnlockMutex(s_lightBaker->mutex);
	}
#endif
}

static void AccumulateLightmapData(int lightmapIndex, const lm_context &ctx, vec4 color)
{
	LightBaker::Lightmap &lightmap = s_lightBaker->lightmaps[lightmapIndex];
	const size_t pixelOffset = ctx.rasterizer.x + ctx.rasterizer.y * world::GetLightmapSize();
	lightmap.color[pixelOffset] += color;
}

static bool DoesSurfaceOccludeLight(const world::Surface &surface)
{
	// Translucent surfaces (e.g. flames) shouldn't occlude.
	// Not handling alpha-testing yet.
	return surface.isValid && (surface.contentFlags & CONTENTS_TRANSLUCENT) == 0 && (surface.surfaceFlags & SURF_ALPHASHADOW) == 0;
}

static vec3 ParseColorString(const char *color)
{
	vec3 rgb;
	sscanf(color, "%f %f %f", &rgb.r, &rgb.g, &rgb.b);

	// Normalize. See q3map ColorNormalize.
	const float max = std::max(rgb.r, std::max(rgb.g, rgb.b));
	return rgb * (1.0f / max);
}

static int CheckEmbreeError(const char *lastFunctionName)
{
	RTCError error = rtcDeviceGetError(s_lightBaker->embreeDevice);

	if (error != RTC_NO_ERROR)
	{
		bx::snprintf(s_lightBaker->errorMessage, LightBaker::maxErrorMessageLen, "Embree error: %s returned %s", lastFunctionName, s_embreeErrorStrings[error]);
		SetStatus(LightBaker::Status::Error);
		return 1;
	}

	return 0;
}

#define CHECK_EMBREE_ERROR(name) { const int r = CheckEmbreeError(#name); if (r) return r; }

static int Thread(void *data)
{
	SetStatus(LightBaker::Status::Running);
	int progress = 0;

	// Count surface triangles for prealloc and so progress can be accurately measured.
	int totalOccluderTriangles = 0, totalLightmappedTriangles = 0;

	for (int si = 0; si < world::GetNumSurfaces(); si++)
	{
		world::Surface surface = world::GetSurface(si);

		if (DoesSurfaceOccludeLight(surface))
		{
			totalOccluderTriangles += surface.nIndices / 3;
		}

		if (surface.isValid && surface.material->lightmapIndex >= 0)
			totalLightmappedTriangles += surface.nIndices / 3;
	}

	// Setup embree.
	s_lightBaker->embreeDevice = rtcNewDevice();
	CHECK_EMBREE_ERROR(rtcNewDevice)
	s_lightBaker->embreeScene = rtcDeviceNewScene(s_lightBaker->embreeDevice, RTC_SCENE_STATIC, RTC_INTERSECT1);
	CHECK_EMBREE_ERROR(rtcDeviceNewScene)
	const std::vector<Vertex> &worldVertices = world::GetVertexBuffer(0);
	unsigned int embreeMesh = rtcNewTriangleMesh(s_lightBaker->embreeScene, RTC_GEOMETRY_STATIC, totalOccluderTriangles, worldVertices.size());
	CHECK_EMBREE_ERROR(rtcNewTriangleMesh);

	auto vertices = (vec4 *)rtcMapBuffer(s_lightBaker->embreeScene, embreeMesh, RTC_VERTEX_BUFFER);
	CHECK_EMBREE_ERROR(rtcMapBuffer);

	for (size_t vi = 0; vi < worldVertices.size(); vi++)
	{
		vertices[vi] = worldVertices[vi].pos;
	}

	rtcUnmapBuffer(s_lightBaker->embreeScene, embreeMesh, RTC_VERTEX_BUFFER);
	CHECK_EMBREE_ERROR(rtcUnmapBuffer);

	auto triangles = (vec3i *)rtcMapBuffer(s_lightBaker->embreeScene, embreeMesh, RTC_INDEX_BUFFER);
	CHECK_EMBREE_ERROR(rtcMapBuffer);
	size_t faceIndex = 0;
	std::vector<unsigned int> skyFaces;

	for (int si = 0; si < world::GetNumSurfaces(); si++)
	{
		world::Surface surface = world::GetSurface(si);

		if (!DoesSurfaceOccludeLight(surface))
			continue;

		for (int si = 0; si < surface.nIndices; si += 3)
		{
			if (surface.material->isSky)
				skyFaces.push_back(faceIndex);

			triangles[faceIndex].x = surface.indices[si + 0];
			triangles[faceIndex].y = surface.indices[si + 1];
			triangles[faceIndex].z = surface.indices[si + 2];
			faceIndex++;
		}
	}

	rtcUnmapBuffer(s_lightBaker->embreeScene, embreeMesh, RTC_INDEX_BUFFER);
	CHECK_EMBREE_ERROR(rtcUnmapBuffer);
	rtcCommit(s_lightBaker->embreeScene);
	CHECK_EMBREE_ERROR(rtcCommit);

	// Allocate lightmap memory.
	const int lightmapSize = world::GetLightmapSize();
	s_lightBaker->lightmaps.resize(world::GetNumLightmaps());

	for (LightBaker::Lightmap &lightmap : s_lightBaker->lightmaps)
	{
		lightmap.color.resize(lightmapSize * lightmapSize);
		memset(lightmap.color.data(), 0, lightmap.color.size());
		lightmap.colorBytes.resize(lightmapSize * lightmapSize * 4);
	}

	// Setup jitter.
	srand(1);
	std::array<vec3, LightBaker::maxSamples> dirJitter, posJitter;

	for (int si = 1; si < s_lightBaker->nSamples; si++) // index 0 left as (0, 0, 0)
	{
		const vec2 dirJitterRange(-0.1f, 0.1f);
		const vec2 posJitterRange(-2.0f, 2.0f);

		for (int i = 0; i < 3; i++)
		{
			dirJitter[si][i] = math::RandomFloat(dirJitterRange.x, dirJitterRange.y);
			posJitter[si][i] = math::RandomFloat(posJitterRange.x, posJitterRange.y);
		}
	}

	// Setup lights.
	vec3 ambientLight;
	std::vector<StaticLight> lights;
	const float pointScale = 7500.0f;
	const float linearScale = 1.0f / 8000.0f;

	for (size_t i = 0; i < world::GetNumEntities(); i++)
	{
		const world::Entity &entity = world::GetEntity(i);
		const char *classname = entity.findValue("classname", "");

		if (!util::Stricmp(classname, "worldspawn"))
		{
			const char *color = entity.findValue("_color");
			const char *ambient = entity.findValue("ambient");
					
			if (color && ambient)
				ambientLight = ParseColorString(color) * (float)atof(ambient);

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
		light.photons = light.intensity * pointScale;
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

			for (size_t j = 0; j < world::GetNumEntities(); j++)
			{
				const world::Entity &targetEntity = world::GetEntity(j);
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

		// Setup envelope. From q3map2 SetupEnvelopes.
		const float falloffTolerance = 1.0f;

		if (!(light.flags & StaticLightFlags::DistanceAttenuation))
		{
			light.envelope = FLT_MAX;
		}
		else if (light.flags & StaticLightFlags::LinearAttenuation)
		{
			light.envelope = light.photons * linearScale - falloffTolerance;
		}
		else
		{
			light.envelope = sqrt(light.photons / falloffTolerance);
		}

		lights.push_back(light);
	}

	// Iterate surfaces.
	const SunLight &sunLight = main::GetSunLight();
	const float maxRayLength = world::GetBounds().toRadius() * 2; // World bounding sphere circumference.
	int nTrianglesProcessed = 0;
	s_lightBaker->nLuxelsProcessed = 0;

	for (int si = 0; si < world::GetNumSurfaces(); si++)
	{
		world::Surface surface = world::GetSurface(si);

		// Ignore surfaces that aren't lightmapped.
		if (!surface.isValid || surface.material->lightmapIndex < 0)
			continue;

		// Iterate surface triangles.
		const std::vector<Vertex> &vertices = world::GetVertexBuffer(surface.vertexBufferIndex);

		for (int ti = 0; ti < surface.nIndices / 3; ti++)
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
				ctx.triangle.uv[i].x = v.texCoord2.x * lightmapSize;
				ctx.triangle.uv[i].y = v.texCoord2.y * lightmapSize;

				// update bounds on lightmap
				uvMin = lm_min2(uvMin, ctx.triangle.uv[i]);
				uvMax = lm_max2(uvMax, ctx.triangle.uv[i]);
			}

			// Calculate area of interest (on lightmap) for conservative rasterization.
			lm_vec2 bbMin = lm_floor2(uvMin);
			lm_vec2 bbMax = lm_ceil2(uvMax);
			ctx.rasterizer.minx = ctx.rasterizer.x = lm_maxi((int)bbMin.x - 1, 0);
			ctx.rasterizer.miny = ctx.rasterizer.y = lm_maxi((int)bbMin.y - 1, 0);
			ctx.rasterizer.maxx = lm_mini((int)bbMax.x + 1, lightmapSize);
			ctx.rasterizer.maxy = lm_mini((int)bbMax.y + 1, lightmapSize);
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

				s_lightBaker->nLuxelsProcessed++;
				vec3 luxelColor = ambientLight;

				// Entity lights.
				const vec3 samplePosition(&ctx.sample.position.x);
				const vec3 sampleNormal(-vec3(&ctx.sample.direction.x));

				for (StaticLight &light : lights)
				{
					float totalAttenuation = 0;

					for (int si = 0; si < s_lightBaker->nSamples; si++)
					{
						vec3 dir(light.position + posJitter[si] - samplePosition); // Jitter light position.
						const vec3 displacement(dir);
						float distance = dir.normalize();
						const float envelope = light.photons;

						if (distance >= envelope)
							continue;

						distance = std::max(distance, 16.0f); // clamp the distance to prevent super hot spots
						float attenuation = 0;

						if (light.flags & StaticLightFlags::LinearAttenuation)
						{
							//attenuation = 1.0f - distance / light.intensity;
							attenuation = std::max(0.0f, light.photons * linearScale - distance);
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
							vec3 normal(light.targetPosition - (light.position + posJitter[si]));
							float coneLength = normal.normalize();

							if (coneLength <= 0)
								coneLength = 64;

							const float radiusByDist = (light.radius + 16) / coneLength;
							const float distByNormal = -vec3::dotProduct(displacement, normal);

							if (distByNormal < 0)
								continue;
							
							const vec3 pointAtDist(light.position + posJitter[si] + normal * distByNormal);
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

						rtcOccluded(s_lightBaker->embreeScene, ray);

						if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
							continue; // hit

						totalAttenuation += attenuation * (1.0f / s_lightBaker->nSamples);
					}
					
					if (totalAttenuation > 0)
						luxelColor += light.color.rgb() * totalAttenuation;
				}

				// Sunlight.
				float attenuation = 0;

				for (int si = 0; si < s_lightBaker->nSamples; si++)
				{
					RTCRay ray;
					const vec3 dir(sunLight.direction + dirJitter[si]); // Jitter light direction.
					const vec3 org(samplePosition + sampleNormal * 0.1f); // push out from the surface a little
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
					rtcIntersect(s_lightBaker->embreeScene, ray);

					if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
					{
						bool hitSky = false;

						for (size_t sfi = 0; sfi < skyFaces.size(); sfi++)
						{
							if (skyFaces[sfi] == ray.primID)
							{
								hitSky = true;
								break;
							}
						}

						if (hitSky)
							attenuation += vec3::dotProduct(sampleNormal, dir) * (1.0f / s_lightBaker->nSamples);
					}
				}

				if (attenuation > 0)
					luxelColor += sunLight.light * attenuation * 255.0f;

				AccumulateLightmapData(surface.material->lightmapIndex, ctx, vec4(luxelColor, 1.0f));
			}

			nTrianglesProcessed += 1;

			// Update progress.
			const int newProgress = int(nTrianglesProcessed / (float)totalLightmappedTriangles * 100.0f);

			if (newProgress != progress)
			{
				progress = newProgress;
				SetStatus(LightBaker::Status::Running, progress);
			}

			// Check for cancelling.
			if (GetStatus() == LightBaker::Status::Cancelled)
				return 1;
		}
	}

	// Average accumulated lightmap data. Different triangles may have written to the same texel.
	for (LightBaker::Lightmap &lightmap : s_lightBaker->lightmaps)
	{
		for (vec4 &color : lightmap.color)
		{
			if (color.a > 1)
				color = vec4(color.rgb() / color.a, 1.0f);
		}
	}

	// Dilate, then copy to LDR buffer.
#define DILATE
#ifdef DILATE
	std::vector<vec4> dilatedLightmap;
	dilatedLightmap.resize(lightmapSize * lightmapSize);
#endif

	for (LightBaker::Lightmap &lightmap : s_lightBaker->lightmaps)
	{
#ifdef DILATE
		lmImageDilate(&lightmap.color[0].x, &dilatedLightmap[0].x, lightmapSize, lightmapSize, 4);
#endif

		for (int i = 0; i < lightmapSize * lightmapSize; i++)
		{
#ifdef DILATE
			const vec4 &src = dilatedLightmap[i];
#else
			const vec4 &src = lightmap.color[i];
#endif
			uint8_t *dest = &lightmap.colorBytes[i * 4];

			// Clamp with color normalization. See q3map ColorToBytes.
			const float max = std::max(src.r, std::max(src.g, src.b));
			vec3 color(src.rgb());

			if (max > 255.0f)
				color *= 255.0f / max;

			dest[0] = uint8_t(color[0]);
			dest[1] = uint8_t(color[1]);
			dest[2] = uint8_t(color[2]);
			dest[3] = 255;
		}
	}

	// Finished rasterization. Stop the worker thread and wait for the main thread to upload the lightmap textures.
	SetStatus(LightBaker::Status::WaitingForTextureUpload);
	return 0;
}

void Start(int nSamples)
{
	if (s_lightBaker.get() || !world::IsLoaded())
		return;

	s_lightBaker = std::make_unique<LightBaker>();
	s_lightBaker->nSamples = math::Clamped(nSamples, 1, LightBaker::maxSamples);
#ifdef USE_LIGHT_BAKER_THREAD
	s_lightBaker->mutex = SDL_CreateMutex();

	if (!s_lightBaker->mutex)
	{
		interface::PrintWarningf("Creating light baker mutex failed. Reason: \"%s\"", SDL_GetError());
		s_lightBaker.reset();
		return;
	}

	s_lightBaker->thread = SDL_CreateThread(Thread, "LightBaker", nullptr);

	if (!s_lightBaker->thread)
	{
		interface::PrintWarningf("Creating light baker thread failed. Reason: \"%s\"", SDL_GetError());
		s_lightBaker.reset();
		return;
	}
#else
	Thread(nullptr);
#endif
}

void Stop()
{
#ifdef USE_LIGHT_BAKER_THREAD
	if (s_lightBaker.get() && s_lightBaker->thread)
	{
		const LightBaker::Status status = GetStatus();

		if (status != LightBaker::Status::Finished && status != LightBaker::Status::Error)
			SetStatus(LightBaker::Status::Cancelled);

		SDL_WaitThread(s_lightBaker->thread, NULL);
	}
#endif

	s_lightBaker.reset();
}

bool IsRunning()
{
	return s_lightBaker.get() && GetStatus() == LightBaker::Status::Running;
}

void Update(int frameNo)
{
	if (!s_lightBaker.get())
		return;

	int progress;
	LightBaker::Status status = GetStatus(&progress);

	if (status == LightBaker::Status::Running)
	{
		main::DebugPrint("Baking lights... %d", progress);
	}
	else if (status == LightBaker::Status::WaitingForTextureUpload)
	{
		// Worker thread has finished. Update lightmap textures.
		const int lightmapSize = world::GetLightmapSize();

		for (int i = 0; i < world::GetNumLightmaps(); i++)
		{
			Texture *lightmap = world::GetLightmap(i);
			lightmap->update(bgfx::makeRef(s_lightBaker->lightmaps[i].colorBytes.data(), lightmapSize * lightmapSize * 4), 0, 0, lightmapSize, lightmapSize);
		}

		s_lightBaker->textureUploadFrameNo = frameNo;
		s_lightBaker->status = LightBaker::Status::Finished;
		interface::Printf("Processed %d luxels\n", s_lightBaker->nLuxelsProcessed);
	}
	else if (status == LightBaker::Status::Finished)
	{
		// Don't stop until we're sure bgfx is finished with the texture memory.
		if (frameNo > s_lightBaker->textureUploadFrameNo + BGFX_NUM_BUFFER_FRAMES)
			Stop();
	}
	else if (status == LightBaker::Status::Error)
	{
		interface::PrintWarningf("%s\n", s_lightBaker->errorMessage);
		Stop();
	}
}

} // namespace light_baker
} // namespace renderer

#endif // USE_LIGHT_BAKER
