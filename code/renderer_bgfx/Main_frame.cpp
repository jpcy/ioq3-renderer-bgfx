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
/*
 * Copyright 2011-2015 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */
#include "Precompiled.h"
#pragma hdrstop

#include "Main.h"

namespace renderer {
namespace main {

struct PushViewFlags
{
	enum
	{
		Sequential = 1<<0
	};
};

struct RenderCameraFlags
{
	enum
	{
		ContainsSkyboxPortal = 1<<0,
		IsSkyboxPortal       = 1<<1,
		SkipUnlitSurfaces    = 1<<2,
		UseClippingPlane     = 1<<3,
		UseStencilTest       = 1<<4
	};
};

struct RenderCameraArgs
{
	VisibilityId visId = VisibilityId::None;
	vec3 pvsPosition;
	vec3 position;
	mat3 rotation;
	Rect rect;
	vec2 fov;
	const uint8_t *areaMask = nullptr;
	Plane clippingPlane;
	int flags = 0;
	const mat4 *customProjectionMatrix = nullptr;
	const FrameBuffer *customFrameBuffer = nullptr;
};

bgfx::ViewId PushView(const FrameBuffer &frameBuffer, uint16_t clearFlags, const mat4 &viewMatrix, const mat4 &projectionMatrix, Rect rect, int flags)
{
	const bgfx::ViewId viewId = s_main->firstFreeViewId++;
	bgfx::setViewClear(viewId, clearFlags);
	bgfx::setViewFrameBuffer(viewId, frameBuffer.handle);
	bgfx::setViewRect(viewId, uint16_t(rect.x), uint16_t(rect.y), uint16_t(rect.w), uint16_t(rect.h));
	bgfx::setViewMode(viewId, (flags & PushViewFlags::Sequential) ? bgfx::ViewMode::Sequential : bgfx::ViewMode::Default);
	bgfx::setViewTransform(viewId, viewMatrix.get(), projectionMatrix.get());
#ifdef _DEBUG
	bgfx::setViewName(viewId, "");
#endif
	return viewId;
}

static void FlushStretchPics()
{
	if (!s_main->stretchPicIndices.empty())
	{
		bgfx::TransientVertexBuffer tvb;
		bgfx::TransientIndexBuffer tib;

		if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, (uint32_t)s_main->stretchPicVertices.size(), &tib, (uint32_t)s_main->stretchPicIndices.size()))
		{
			WarnOnce(WarnOnceId::TransientBuffer);
		}
		else
		{
			memcpy(tvb.data, &s_main->stretchPicVertices[0], sizeof(Vertex) * s_main->stretchPicVertices.size());
			memcpy(tib.data, &s_main->stretchPicIndices[0], sizeof(uint16_t) * s_main->stretchPicIndices.size());
			s_main->time = interface::GetTime();
			s_main->floatTime = s_main->time * 0.001f;
			s_main->uniforms->renderMode.set(vec4::empty);
			s_main->uniforms->dynamicLight_Num_Intensity.set(vec4::empty);
			s_main->matUniforms->nDeforms.set(vec4(0, 0, 0, 0));
			s_main->matUniforms->time.set(vec4(s_main->stretchPicMaterial->setTime(s_main->floatTime), 0, 0, 0));

			if (s_main->stretchPicViewId == UINT8_MAX)
			{
				s_main->stretchPicViewId = PushView(s_main->defaultFb, BGFX_CLEAR_NONE, mat4::identity, mat4::orthographicProjection(0, (float)window::GetWidth(), 0, (float)window::GetHeight(), -1, 1), Rect(0, 0, window::GetWidth(), window::GetHeight()), PushViewFlags::Sequential);
#ifdef _DEBUG
				bgfx::setViewName(s_main->stretchPicViewId, "StretchPic");
#endif
			}

			for (const MaterialStage &stage : s_main->stretchPicMaterial->stages)
			{
				if (!stage.active)
					continue;

				stage.setShaderUniforms(s_main->matStageUniforms.get());
				stage.setTextureSamplers(s_main->matStageUniforms.get());
				uint64_t state = BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | stage.getState();

				// Depth testing and writing should always be off for 2D drawing.
				state &= ~BGFX_STATE_DEPTH_TEST_MASK;
				state &= ~BGFX_STATE_DEPTH_WRITE;

				bgfx::setState(state);
				bgfx::setVertexBuffer(0, &tvb);
				bgfx::setIndexBuffer(&tib);
				bgfx::submit(s_main->stretchPicViewId, s_main->shaderPrograms[ShaderProgramId::Generic].handle);
			}
		}
	}

	s_main->stretchPicVertices.clear();
	s_main->stretchPicIndices.clear();
}

void DrawAxis(vec3 position)
{
	s_main->sceneDebugAxis.push_back(position);
}

void DrawBounds(const Bounds &bounds)
{
	s_main->sceneDebugBounds.push_back(bounds);
}

void DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex)
{
	DrawStretchPicGradient(x, y, w, h, s1, t1, s2, t2, materialIndex, s_main->stretchPicColor);
}

void DrawStretchPicGradient(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex, vec4 gradientColor)
{
	Material *mat = s_main->materialCache->getMaterial(materialIndex);

	if (s_main->stretchPicMaterial != mat)
	{
		FlushStretchPics();
		s_main->stretchPicMaterial = mat;
	}

	auto firstVertex = (const uint16_t)s_main->stretchPicVertices.size();
	size_t firstIndex = s_main->stretchPicIndices.size();
	s_main->stretchPicVertices.resize(s_main->stretchPicVertices.size() + 4);
	s_main->stretchPicIndices.resize(s_main->stretchPicIndices.size() + 6);
	Vertex *v = &s_main->stretchPicVertices[firstVertex];
	uint16_t *i = &s_main->stretchPicIndices[firstIndex];
	v[0].pos = vec3(x, y, 0);
	v[1].pos = vec3(x + w, y, 0);
	v[2].pos = vec3(x + w, y + h, 0);
	v[3].pos = vec3(x, y + h, 0);
	v[0].setTexCoord(s1, t1);
	v[1].setTexCoord(s2, t1);
	v[2].setTexCoord(s2, t2);
	v[3].setTexCoord(s1, t2);
	v[0].setColor(util::ToLinear(s_main->stretchPicColor)); v[1].color = v[0].color;
	v[2].setColor(util::ToLinear(gradientColor)); v[3].color = v[2].color;
	i[0] = firstVertex + 3; i[1] = firstVertex + 0; i[2] = firstVertex + 2;
	i[3] = firstVertex + 2; i[4] = firstVertex + 0; i[5] = firstVertex + 1;
}

void DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty)
{
	if (!math::IsPowerOfTwo(cols) || !math::IsPowerOfTwo(rows))
	{
		interface::Error("Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, 4, &tib, 6))
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	FlushStretchPics();
	s_main->stretchPicViewId = UINT8_MAX;
	UploadCinematic(w, h, cols, rows, data, client, dirty);
	auto vertices = (Vertex *)tvb.data;
	vertices[0].pos = { 0, 0, 0 }; vertices[0].setTexCoord(0, 0);
	vertices[1].pos = { 1, 0, 0 }; vertices[1].setTexCoord(1, 0);
	vertices[2].pos = { 1, 1, 0 }; vertices[2].setTexCoord(1, 1);
	vertices[3].pos = { 0, 1, 0 }; vertices[3].setTexCoord(0, 1);
	auto indices = (uint16_t *)tib.data;
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 2; indices[4] = 3; indices[5] = 0;
	bgfx::setVertexBuffer(0, &tvb);
	bgfx::setIndexBuffer(&tib);
	bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, g_textureCache->getScratch(size_t(client))->getHandle());
	
	if (g_hardwareGammaEnabled)
	{
		s_main->matStageUniforms->color.set(vec4(g_identityLight, g_identityLight, g_identityLight, 1));
	}
	else
	{
		s_main->matStageUniforms->color.set(vec4::white);
	}

	bgfx::setState(BGFX_STATE_RGB_WRITE);
	const bgfx::ViewId viewId = PushView(s_main->defaultFb, BGFX_CLEAR_NONE, mat4::identity, mat4::orthographicProjection(0, 1, 0, 1, -1, 1), Rect(x, y, w, h), PushViewFlags::Sequential);
#ifdef _DEBUG
	bgfx::setViewName(viewId, "StretchRaw");
#endif
	bgfx::submit(viewId, s_main->shaderPrograms[ShaderProgramId::TextureColor].handle);
}

// From bgfx screenSpaceQuad.
void RenderScreenSpaceQuad(const char *viewName, const FrameBuffer &frameBuffer, ShaderProgramId::Enum program, uint64_t state, uint16_t clearFlags, bool originBottomLeft, Rect rect)
{
	const uint32_t nVerts = 3;
	if (bgfx::getAvailTransientVertexBuffer(nVerts, Vertex::decl) < nVerts)
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	if (!rect.w) rect.w = window::GetWidth();
	if (!rect.h) rect.h = window::GetHeight();
	const float width = 1.0f;
	const float height = 1.0f;
	const float zz = 0.0f;
	const float minx = -width;
	const float maxx =  width;
	const float miny = 0.0f;
	const float maxy = height*2.0f;
	const float texelHalfW = s_main->halfTexelOffset / rect.w;
	const float texelHalfH = s_main->halfTexelOffset / rect.h;
	const float minu = -1.0f + texelHalfW;
	const float maxu =  1.0f + texelHalfW;
	float minv = texelHalfH;
	float maxv = 2.0f + texelHalfH;

	if (originBottomLeft)
	{
		float temp = minv;
		minv = maxv;
		maxv = temp;
		minv -= 1.0f;
		maxv -= 1.0f;
	}

	bgfx::TransientVertexBuffer vb;
	bgfx::allocTransientVertexBuffer(&vb, nVerts, Vertex::decl);
	auto vertices = (Vertex *)vb.data;
	vertices[0].pos = vec3(minx, miny, zz);
	vertices[0].setColor(vec4::white);
	vertices[0].setTexCoord(minu, minv);
	vertices[1].pos = vec3(maxx, miny, zz);
	vertices[1].setColor(vec4::white);
	vertices[1].setTexCoord(maxu, minv);
	vertices[2].pos = vec3(maxx, maxy, zz);
	vertices[2].setColor(vec4::white);
	vertices[2].setTexCoord(maxu, maxv);
	bgfx::setVertexBuffer(0, &vb);
	bgfx::setState(state);
	const bgfx::ViewId viewId = PushView(frameBuffer, clearFlags, mat4::identity, mat4::orthographicProjection(0, 1, 0, 1, -1, 1), rect);
#ifdef _DEBUG
	bgfx::setViewName(viewId, viewName);
#else
	BX_UNUSED(viewName);
#endif
	bgfx::submit(viewId, s_main->shaderPrograms[program].handle);
}

static void Blit(const char *viewName, bgfx::TextureHandle source, bgfx::TextureHandle dest)
{
	const bgfx::ViewId viewId = PushView(s_main->defaultFb, BGFX_CLEAR_NONE, mat4::identity, mat4::identity, Rect());
	bgfx::blit(viewId, dest, 0, 0, source);
#ifdef _DEBUG
	bgfx::setViewName(viewId, viewName);
#else
	BX_UNUSED(viewName);
#endif
}

static void RenderDebugDraw(bgfx::TextureHandle texture, int x = 0, int y = 0, ShaderProgramId::Enum program = ShaderProgramId::Texture)
{
	bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, texture);
	RenderScreenSpaceQuad("DebugDraw", s_main->defaultFb, program, BGFX_STATE_RGB_WRITE, BGFX_CLEAR_NONE, s_main->isTextureOriginBottomLeft, Rect(g_cvars.debugDrawSize.getInt() * x, g_cvars.debugDrawSize.getInt() * y, g_cvars.debugDrawSize.getInt(), g_cvars.debugDrawSize.getInt()));
}

static void ClampEntityLight(vec3 *light)
{
	assert(light);
	float max = std::max(light->r, std::max(light->g, light->b));

	if (max > 255.0f)
	{
		max = 255.0f / max;
		light->r *= max;
		light->g *= max;
		light->b *= max;
	}
}

static void SetupEntityLighting(Entity *entity)
{
	assert(entity);

	// Trace a sample point down to find ambient light.
	vec3 lightPosition;
	
	if (entity->flags & EntityFlags::LightingPosition)
	{
		// Seperate lightOrigins are needed so an object that is sinking into the ground can still be lit, and so multi-part models can be lit identically.
		lightPosition = entity->lightingPosition;
	}
	else
	{
		lightPosition = entity->position;
	}

	// If not a world scene, only use dynamic lights (menu system, etc.)
	if (s_main->isWorldCamera && world::HasLightGrid())
	{
		world::SampleLightGrid(lightPosition, &entity->ambientLight, &entity->directedLight, &entity->lightDir);
	}
	else
	{
		entity->ambientLight = vec3(g_identityLight * 150);
		entity->directedLight = vec3(g_identityLight * 150);
		entity->lightDir = s_main->sunLight.direction;
	}

	// Bonus items and view weapons have a fixed minimum add.
	//if (entity->e.renderfx & RF_MINLIGHT)
	{
		// Give everything a minimum light add.
		entity->ambientLight += vec3(g_identityLight * 32);
	}

	// Modify the light by dynamic lights.
	if (!s_main->isWorldCamera)
	{
		s_main->dlightManager->contribute(s_main->frameNo, lightPosition, &entity->directedLight, &entity->lightDir);
	}

	ClampEntityLight(&entity->ambientLight);
	ClampEntityLight(&entity->directedLight);
	entity->lightDir.normalize();
}

static void RenderRailCore(vec3 start, vec3 end, vec3 up, float length, float spanWidth, Material *mat, vec4 color, Entity *entity)
{
	const uint32_t nVertices = 4, nIndices = 6;
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, nVertices, &tib, nIndices)) 
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	auto vertices = (Vertex *)tvb.data;
	vertices[0].pos = start + up * spanWidth;
	vertices[1].pos = start + up * -spanWidth;
	vertices[2].pos = end + up * spanWidth;
	vertices[3].pos = end + up * -spanWidth;

	const float t = length / 256.0f;
	vertices[0].setTexCoord(0, 0);
	vertices[1].setTexCoord(0, 1);
	vertices[2].setTexCoord(t, 0);
	vertices[3].setTexCoord(t, 1);

	vertices[0].setColor(util::ToLinear(vec4(color.xyz() * 0.25f, 1)));
	vertices[1].setColor(util::ToLinear(color));
	vertices[2].setColor(util::ToLinear(color));
	vertices[3].setColor(util::ToLinear(color));

	auto indices = (uint16_t *)tib.data;
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 2; indices[4] = 1; indices[5] = 3;

	DrawCall dc;
	dc.dynamicLighting = false;
	dc.entity = entity;
	dc.fogIndex = s_main->isWorldCamera ? world::FindFogIndex(entity->position, entity->radius) : -1;
	dc.material = mat;
	dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
	dc.vb.transientHandle = tvb;
	dc.vb.nVertices = nVertices;
	dc.ib.transientHandle = tib;
	dc.ib.nIndices = nIndices;
	s_main->drawCalls.push_back(dc);
}

static void RenderLightningEntity(vec3 viewPosition, mat3 viewRotation, Entity *entity)
{
	const vec3 start(entity->position), end(entity->oldPosition);
	vec3 dir = (end - start);
	const float length = dir.normalize();

	// Compute side vector.
	const vec3 v1 = (start - viewPosition).normal();
	const vec3 v2 = (end - viewPosition).normal();
	vec3 right = vec3::crossProduct(v1, v2).normal();

	for (int i = 0; i < 4; i++)
	{
		RenderRailCore(start, end, right, length, g_cvars.railCoreWidth.getFloat(), s_main->materialCache->getMaterial(entity->customMaterial), entity->materialColor, entity);
		right = right.rotatedAroundDirection(dir, 45);
	}
}

static void RenderRailCoreEntity(vec3 viewPosition, mat3 viewRotation, Entity *entity)
{
	const vec3 start(entity->oldPosition), end(entity->position);
	vec3 dir = (end - start);
	const float length = dir.normalize();

	// Compute side vector.
	const vec3 v1 = (start - viewPosition).normal();
	const vec3 v2 = (end - viewPosition).normal();
	const vec3 right = vec3::crossProduct(v1, v2).normal();

	RenderRailCore(start, end, right, length, g_cvars.railCoreWidth.getFloat(), s_main->materialCache->getMaterial(entity->customMaterial), entity->materialColor, entity);
}

static void RenderRailRingsEntity(Entity *entity)
{
	const vec3 start(entity->oldPosition), end(entity->position);
	vec3 dir = (end - start);
	const float length = dir.normalize();
	vec3 right, up;
	dir.toNormalVectors(&right, &up);
	dir *= g_cvars.railSegmentLength.getFloat();
	int nSegments = (int)std::max(1.0f, length / g_cvars.railSegmentLength.getFloat());

	if (nSegments > 1)
		nSegments--;

	if (!nSegments)
		return;

	const float scale = 0.25f;
	const float spanWidth = g_cvars.railWidth.getFloat();
	vec3 positions[4];

	for (int i = 0; i < 4; i++)
	{
		const float c = cos(DEG2RAD(45 + i * 90));
		const float s = sin(DEG2RAD(45 + i * 90));
		positions[i] = start + (right * c + up * s) * scale * spanWidth;

		if (nSegments)
		{
			// Offset by 1 segment if we're doing a long distance shot.
			positions[i] += dir;
		}
	}

	const uint32_t nVertices = 4 * nSegments, nIndices = 6 * nSegments;
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, nVertices, &tib, nIndices)) 
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	for (int i = 0; i < nSegments; i++)
	{
		for (int j = 0; j < 4; j++ )
		{
			auto vertex = &((Vertex *)tvb.data)[i * 4 + j];
			vertex->pos = positions[j];
			vertex->setTexCoord(j < 2, j && j != 3);
			vertex->setColor(entity->materialColor);
			positions[j] += dir;
		}

		auto index = &((uint16_t *)tib.data)[i * 6];
		const uint16_t offset = i * 4;
		index[0] = offset + 0; index[1] = offset + 1; index[2] = offset + 3;
		index[3] = offset + 3; index[4] = offset + 1; index[5] = offset + 2;
	}

	DrawCall dc;
	dc.dynamicLighting = false;
	dc.entity = entity;
	dc.fogIndex = s_main->isWorldCamera ? world::FindFogIndex(entity->position, entity->radius) : -1;
	dc.material = s_main->materialCache->getMaterial(entity->customMaterial);
	dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
	dc.vb.transientHandle = tvb;
	dc.vb.nVertices = nVertices;
	dc.ib.transientHandle = tib;
	dc.ib.nIndices = nIndices;
	s_main->drawCalls.push_back(dc);
}

static void RenderSpriteEntity(mat3 viewRotation, Entity *entity)
{
	// Calculate the positions for the four corners.
	vec3 left, up;

	if (entity->angle == 0)
	{
		left = viewRotation[1] * entity->radius;
		up = viewRotation[2] * entity->radius;
	}
	else
	{
		const float ang = (float)M_PI * entity->angle / 180.0f;
		const float s = sin(ang);
		const float c = cos(ang);
		left = viewRotation[1] * (c * entity->radius) + viewRotation[2] * (-s * entity->radius);
		up = viewRotation[2] * (c * entity->radius) + viewRotation[1] * (s * entity->radius);
	}

	if (s_main->isCameraMirrored)
		left = -left;

	const uint32_t nVertices = 4, nIndices = 6;
	bgfx::TransientVertexBuffer tvb;
	bgfx::TransientIndexBuffer tib;

	if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, nVertices, &tib, nIndices)) 
	{
		WarnOnce(WarnOnceId::TransientBuffer);
		return;
	}

	auto vertices = (Vertex *)tvb.data;
	vertices[0].pos = entity->position + left + up;
	vertices[1].pos = entity->position - left + up;
	vertices[2].pos = entity->position - left - up;
	vertices[3].pos = entity->position + left - up;

	// Constant normal all the way around.
	vertices[0].setNormal(-viewRotation[0]);
	vertices[1].setNormal(-viewRotation[0]);
	vertices[2].setNormal(-viewRotation[0]);
	vertices[3].setNormal(-viewRotation[0]);

	// Standard square texture coordinates.
	vertices[0].setTexCoord(0, 0, 0, 0);
	vertices[1].setTexCoord(1, 0, 1, 0);
	vertices[2].setTexCoord(1, 1, 1, 1);
	vertices[3].setTexCoord(0, 1, 0, 1);

	// Constant color all the way around.
	for (int i = 0; i < 4; i++)
		vertices[i].setColor(util::ToLinear(entity->materialColor));

	auto indices = (uint16_t *)tib.data;
	indices[0] = 0; indices[1] = 1; indices[2] = 3;
	indices[3] = 3; indices[4] = 1; indices[5] = 2;

	DrawCall dc;
	dc.dynamicLighting = false;
	dc.entity = entity;
	dc.fogIndex = s_main->isWorldCamera ? world::FindFogIndex(entity->position, entity->radius) : -1;
	dc.material = s_main->materialCache->getMaterial(entity->customMaterial);
	dc.softSpriteDepth = entity->radius / 2.0f;
	dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
	dc.vb.transientHandle = tvb;
	dc.vb.nVertices = nVertices;
	dc.ib.transientHandle = tib;
	dc.ib.nIndices = nIndices;
	s_main->drawCalls.push_back(dc);
}

static void RenderEntity(vec3 viewPosition, mat3 viewRotation, Frustum cameraFrustum, Entity *entity)
{
	assert(entity);

	// Calculate the viewer origin in the model's space.
	// Needed for fog, specular, and environment mapping.
	const vec3 delta = viewPosition - entity->position;

	// Compensate for scale in the axes if necessary.
	float axisLength = 1;

	if (entity->nonNormalizedAxes)
	{
		axisLength = 1.0f / entity->rotation[0].length();
	}

	entity->localViewPosition =
	{
		vec3::dotProduct(delta, entity->rotation[0]) * axisLength,
		vec3::dotProduct(delta, entity->rotation[1]) * axisLength,
		vec3::dotProduct(delta, entity->rotation[2]) * axisLength
	};

	switch (entity->type)
	{
	case EntityType::Beam:
		break;

	case EntityType::Lightning:
		RenderLightningEntity(viewPosition, viewRotation, entity);
		break;

	case EntityType::Model:
		if (entity->handle == 0)
		{
			s_main->sceneDebugAxis.push_back(entity->position);
		}
		else
		{
			Model *model = s_main->modelCache->getModel(entity->handle);

			if (model->isCulled(entity, cameraFrustum))
				break;

			SetupEntityLighting(entity);
			model->render(s_main->sceneRotation, &s_main->drawCalls, entity);
		}
		break;
	
	case EntityType::RailCore:
		RenderRailCoreEntity(viewPosition, viewRotation, entity);
		break;

	case EntityType::RailRings:
		RenderRailRingsEntity(entity);
		break;

	case EntityType::Sprite:
		if (cameraFrustum.clipSphere(entity->position, entity->radius) == Frustum::ClipResult::Outside)
			break;

		RenderSpriteEntity(viewRotation, entity);
		break;

	default:
		break;
	}
}

static void RenderPolygons()
{
	if (s_main->scenePolygons.empty())
		return;

	// Sort polygons by material and fogIndex for batching.
	for (Main::Polygon &polygon : s_main->scenePolygons)
	{
		s_main->sortedScenePolygons.push_back(&polygon);
	}

	std::sort(s_main->sortedScenePolygons.begin(), s_main->sortedScenePolygons.end(), [](Main::Polygon *a, Main::Polygon *b)
	{
		if (a->material->index < b->material->index)
			return true;
		else if (a->material->index == b->material->index)
		{
			return a->fogIndex < b->fogIndex;
		}

		return false;
	});

	size_t batchStart = 0;

	for (;;)
	{
		uint32_t nVertices = 0, nIndices = 0;
		size_t batchEnd;

		// Find the last polygon index that matches the current material and fog. Count geo as we go.
		for (batchEnd = batchStart; batchEnd < s_main->sortedScenePolygons.size(); batchEnd++)
		{
			const Main::Polygon *p = s_main->sortedScenePolygons[batchEnd];

			if (p->material != s_main->sortedScenePolygons[batchStart]->material || p->fogIndex != s_main->sortedScenePolygons[batchStart]->fogIndex)
				break;

			nVertices += p->nVertices;
			nIndices += (p->nVertices - 2) * 3;
		}

		batchEnd = std::max(batchStart, batchEnd - 1);

		// Got a range of polygons to batch. Build a draw call.
		bgfx::TransientVertexBuffer tvb;
		bgfx::TransientIndexBuffer tib;

		if (!bgfx::allocTransientBuffers(&tvb, Vertex::decl, nVertices, &tib, nIndices))
		{
			WarnOnce(WarnOnceId::TransientBuffer);
			break;
		}

		auto vertices = (Vertex *)tvb.data;
		auto indices = (uint16_t *)tib.data;
		uint32_t currentVertex = 0, currentIndex = 0;

		for (size_t i = batchStart; i <= batchEnd; i++)
		{
			const Main::Polygon *p = s_main->sortedScenePolygons[i];
			const uint32_t firstVertex = currentVertex;

			for (size_t j = 0; j < p->nVertices; j++)
			{
				Vertex &v = vertices[currentVertex++];
				const polyVert_t &pv = s_main->scenePolygonVertices[p->firstVertex + j];
				v.pos = pv.xyz;
				v.setTexCoord(pv.st[0], pv.st[1]);
				v.setColor(vec4::fromBytes(pv.modulate));
			}

			for (size_t j = 0; j < p->nVertices - 2; j++)
			{
				indices[currentIndex++] = firstVertex + 0;
				indices[currentIndex++] = firstVertex + uint16_t(j) + 1;
				indices[currentIndex++] = firstVertex + uint16_t(j) + 2;
			}
		}

		DrawCall dc;
		dc.dynamicLighting = false; // No dynamic lighting on decals.
		dc.fogIndex = s_main->sortedScenePolygons[batchStart]->fogIndex;
		dc.material = s_main->sortedScenePolygons[batchStart]->material;
		dc.vb.type = dc.ib.type = DrawCall::BufferType::Transient;
		dc.vb.transientHandle = tvb;
		dc.vb.nVertices = nVertices;
		dc.ib.transientHandle = tib;
		dc.ib.nIndices = nIndices;
		s_main->drawCalls.push_back(dc);

		// Iterate.
		batchStart = batchEnd + 1;

		if (batchStart >= s_main->sortedScenePolygons.size())
			break;
	}
}

static void SetDrawCallGeometry(const DrawCall &dc)
{
	assert(dc.vb.nVertices);
	assert(dc.ib.nIndices);

	if (dc.vb.type == DrawCall::BufferType::Static)
	{
		bgfx::setVertexBuffer(0, dc.vb.staticHandle, dc.vb.firstVertex, dc.vb.nVertices);
	}
	else if (dc.vb.type == DrawCall::BufferType::Dynamic)
	{
		bgfx::setVertexBuffer(0, dc.vb.dynamicHandle, dc.vb.firstVertex, dc.vb.nVertices);
	}
	else if (dc.vb.type == DrawCall::BufferType::Transient)
	{
		bgfx::setVertexBuffer(0, &dc.vb.transientHandle, dc.vb.firstVertex, dc.vb.nVertices);
	}

	if (dc.ib.type == DrawCall::BufferType::Static)
	{
		bgfx::setIndexBuffer(dc.ib.staticHandle, dc.ib.firstIndex, dc.ib.nIndices);
	}
	else if (dc.ib.type == DrawCall::BufferType::Dynamic)
	{
		bgfx::setIndexBuffer(dc.ib.dynamicHandle, dc.ib.firstIndex, dc.ib.nIndices);
	}
	else if (dc.ib.type == DrawCall::BufferType::Transient)
	{
		bgfx::setIndexBuffer(&dc.ib.transientHandle, dc.ib.firstIndex, dc.ib.nIndices);
	}
}

static void RenderToStencil(const bgfx::ViewId viewId)
{
	const uint32_t stencilWrite = BGFX_STENCIL_TEST_ALWAYS | BGFX_STENCIL_FUNC_REF(1) | BGFX_STENCIL_FUNC_RMASK(0xff) | BGFX_STENCIL_OP_FAIL_S_REPLACE | BGFX_STENCIL_OP_FAIL_Z_REPLACE | BGFX_STENCIL_OP_PASS_Z_REPLACE;
	s_main->currentEntity = nullptr;

	for (DrawCall &dc : s_main->drawCalls)
	{
		Material *mat = dc.material->remappedShader ? dc.material->remappedShader : dc.material;
		s_main->uniforms->depthRangeEnabled.set(vec4::empty);
		s_main->uniforms->depthRange.set(vec4::empty);
		s_main->matUniforms->time.set(vec4(mat->setTime(s_main->floatTime), 0, 0, 0));
		mat->setDeformUniforms(s_main->matUniforms.get());
		s_main->matStageUniforms->alphaTest.set(vec4::empty);
		SetDrawCallGeometry(dc);
		bgfx::setTransform(dc.modelMatrix.get());
		uint64_t state = BGFX_STATE_RGB_WRITE | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_DEPTH_WRITE;

		if (IsMsaa(s_main->aa))
			state |= BGFX_STATE_MSAA;

		// Grab the cull state. Doesn't matter which stage, since it's global to the material.
		state |= mat->stages[0].getState() & BGFX_STATE_CULL_MASK;

		bgfx::setState(state);
		bgfx::setStencil(stencilWrite);
		bgfx::submit(viewId, s_main->shaderPrograms[ShaderProgramId::Depth].handle);
	}
}

static vec2 CalculateDepthRange(VisibilityId visId, vec3 position)
{
	const float zMin = 4;
	float zMax = 2048;

	if (s_main->isWorldCamera)
	{
		// Use dynamic z max.
		zMax = world::GetBounds(visId).calculateFarthestCornerDistance(position);
	}

	return vec2(zMin, zMax);
}

static void RenderCamera(const RenderCameraArgs &args)
{
	const float polygonDepthOffset = -0.001f;
	const uint32_t stencilTest = BGFX_STENCIL_TEST_EQUAL | BGFX_STENCIL_FUNC_REF(1) | BGFX_STENCIL_FUNC_RMASK(1) | BGFX_STENCIL_OP_FAIL_S_KEEP | BGFX_STENCIL_OP_FAIL_Z_KEEP | BGFX_STENCIL_OP_PASS_Z_KEEP;

	s_main->isWorldCamera = args.visId != VisibilityId::None;
	const bool isProbe = args.visId == VisibilityId::Probe;

	// Update visibility for this PVS position.
	// Probes do this externally.
	if (s_main->isWorldCamera && !isProbe)
	{
		world::UpdateVisibility(args.visId, args.pvsPosition, args.areaMask);
	}

	const vec2 depthRange = CalculateDepthRange(args.visId, args.pvsPosition);
	s_main->lastCameraDepthRange = depthRange;

	// Setup camera transform.
	const mat4 viewMatrix = s_main->toOpenGlMatrix * mat4::view(args.position, args.rotation);
	const mat4 projectionMatrix = args.customProjectionMatrix ? *args.customProjectionMatrix : mat4::perspectiveProjection(args.fov.x, args.fov.y, depthRange.x, depthRange.y);
	const mat4 vpMatrix(projectionMatrix * viewMatrix);
	const Frustum cameraFrustum(vpMatrix);

	// The main camera can have a single portal camera and a single reflection camera. No deep recursion.
	if (args.visId == VisibilityId::Main)
	{
		s_main->mainCameraTransform.position = args.position;
		s_main->mainCameraTransform.rotation = args.rotation;

		// Render a reflection camera if there's a reflecting surface visible.
		if (s_main->waterReflectionsEnabled)
		{
			Transform reflectionCamera;
			Plane reflectionPlane;

			if (world::CalculateReflectionCamera(args.visId, args.position, args.rotation, vpMatrix, &reflectionCamera, &reflectionPlane))
			{
				// Write stencil mask first.
				s_main->drawCalls.clear();
				world::RenderReflective(args.visId, &s_main->drawCalls);
				assert(!s_main->drawCalls.empty());
				const bgfx::ViewId viewId = PushView(s_main->sceneFb, BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, viewMatrix, projectionMatrix, args.rect);
#ifdef _DEBUG
				bgfx::setViewName(viewId, "ReflectionStencilMask");
#endif
				RenderToStencil(viewId);

				// Render to the scene frame buffer with stencil testing.
				s_main->isCameraMirrored = true;
				RenderCameraArgs reflectionArgs;
				reflectionArgs.areaMask = args.areaMask;
				reflectionArgs.clippingPlane = reflectionPlane;
				reflectionArgs.flags = args.flags | RenderCameraFlags::UseClippingPlane | RenderCameraFlags::UseStencilTest;
				reflectionArgs.fov = args.fov;
				reflectionArgs.position = reflectionCamera.position;
				reflectionArgs.pvsPosition = args.pvsPosition;
				reflectionArgs.rect = args.rect;
				reflectionArgs.rotation = reflectionCamera.rotation;
				reflectionArgs.visId = VisibilityId::Reflection;
				RenderCamera(reflectionArgs);
				s_main->isCameraMirrored = false;

				// Blit the scene frame buffer to the reflection frame buffer.
				bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, bgfx::getTexture(s_main->sceneFb.handle));
				RenderScreenSpaceQuad("Reflection", s_main->reflectionFb, ShaderProgramId::Texture, BGFX_STATE_RGB_WRITE, BGFX_CLEAR_NONE, s_main->isTextureOriginBottomLeft);
			}
		}

		// Render a portal camera if there's a portal surface visible.
		vec3 portalPvsPosition;
		Transform portalCamera;
		Plane portalPlane;
		bool isCameraMirrored;

		if (world::CalculatePortalCamera(args.visId, args.position, args.rotation, vpMatrix, s_main->sceneEntities, &portalPvsPosition, &portalCamera, &isCameraMirrored, &portalPlane))
		{
			// Write stencil mask first.
			s_main->drawCalls.clear();
			world::RenderPortal(args.visId, &s_main->drawCalls);
			assert(!s_main->drawCalls.empty());
			const bgfx::ViewId viewId = PushView(s_main->fastPathEnabled ? s_main->defaultFb : s_main->sceneFb, BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, viewMatrix, projectionMatrix, args.rect);
#ifdef _DEBUG
			bgfx::setViewName(viewId, "PortalStencilMask");
#endif
			RenderToStencil(viewId);

			// Render the portal camera with stencil testing.
			s_main->isCameraMirrored = isCameraMirrored;
			RenderCameraArgs portalArgs;
			portalArgs.areaMask = args.areaMask;
			portalArgs.clippingPlane = portalPlane;
			portalArgs.flags = args.flags | RenderCameraFlags::UseClippingPlane | RenderCameraFlags::UseStencilTest;
			portalArgs.fov = args.fov;
			portalArgs.position = portalCamera.position;
			portalArgs.pvsPosition = portalPvsPosition;
			portalArgs.rect = args.rect;
			portalArgs.rotation = portalCamera.rotation;
			portalArgs.visId = VisibilityId::Portal;
			RenderCamera(portalArgs);
			s_main->isCameraMirrored = false;
		}
	}

	// Build draw calls. Order doesn't matter.
	s_main->drawCalls.clear();

	if (s_main->isWorldCamera)
	{
		// If dealing with skybox portals, only render the sky to the skybox portal, not the camera containing it.
		if ((args.flags & RenderCameraFlags::IsSkyboxPortal) || (args.flags & RenderCameraFlags::ContainsSkyboxPortal) == 0)
		{
			for (size_t i = 0; i < world::GetNumSkySurfaces(args.visId); i++)
			{
				Sky_Render(&s_main->drawCalls, args.position, depthRange.y, world::GetSkySurface(args.visId, i));
			}
		}

		world::Render(args.visId, &s_main->drawCalls, s_main->sceneRotation);
	}

	for (Entity &entity : s_main->sceneEntities)
	{
		if (args.visId == VisibilityId::Main && (entity.flags & EntityFlags::ThirdPerson) != 0)
			continue;

		if (args.visId != VisibilityId::Main && (entity.flags & EntityFlags::FirstPerson) != 0)
			continue;

		s_main->currentEntity = &entity;
		RenderEntity(args.position, args.rotation, cameraFrustum, &entity);
		s_main->currentEntity = nullptr;
	}

	RenderPolygons();

	if (s_main->drawCalls.empty())
		return;

	// Sort draw calls.
	std::sort(s_main->drawCalls.begin(), s_main->drawCalls.end());

	// Set plane clipping.
	if (args.flags & RenderCameraFlags::UseClippingPlane)
	{
		s_main->uniforms->portalClipEnabled.set(vec4(1, 0, 0, 0));
		s_main->uniforms->portalPlane.set(args.clippingPlane.toVec4());
	}
	else
	{
		s_main->uniforms->portalClipEnabled.set(vec4::empty);
	}

	// Render to shadow map. Probes skip this.
	if (s_main->sunLightEnabled && s_main->isWorldCamera && !isProbe)
	{
		Bounds bounds(world::GetBounds());
		vec3 eye;
		vec3 center = -s_main->sunLight.direction;
		vec3 up(0.0f, 1.0f, 0.0f);
		mat4 shadowViewMatrix;
		bx::mtxLookAt((float *)&shadowViewMatrix, (float *)&eye, (float *)&center, (float *)&up);
		std::array<vec3, 8> corners = bounds.toVertices();
		bounds.setupForAddingPoints();

		for (size_t i = 0; i < corners.size(); i++)
		{
			bounds.addPoint(shadowViewMatrix.transform(corners[i]));
		}

		mat4 shadowProjectionMatrix;
		bx::mtxOrtho((float *)&shadowProjectionMatrix, bounds.min.x, bounds.max.x, bounds.min.y, bounds.max.y, bounds.min.z, bounds.max.z, 0.0f, bgfx::getCaps()->homogeneousDepth);
		const bgfx::ViewId viewId = PushView(s_main->shadowMapFb, BGFX_CLEAR_DEPTH, shadowViewMatrix, shadowProjectionMatrix, Rect(0, 0, s_main->shadowMapSize, s_main->shadowMapSize));
#ifdef _DEBUG
		bgfx::setViewName(viewId, "ShadowMap");
#endif

		for (DrawCall &dc : s_main->drawCalls)
		{
			// Material remapping.
			Material *mat = dc.material->remappedShader ? dc.material->remappedShader : dc.material;

			if (mat->sort != MaterialSort::Opaque || mat->numUnfoggedPasses == 0 || dc.flags & DrawCallFlags::Sky)
				continue;

			// Don't render first person models.
			if (dc.entity && (dc.entity->flags & EntityFlags::FirstPerson))
				continue;

			s_main->currentEntity = dc.entity;
			s_main->matUniforms->time.set(vec4(mat->setTime(s_main->floatTime), 0, 0, 0));
			s_main->uniforms->depthRangeEnabled.set(vec4::empty);
			mat->setDeformUniforms(s_main->matUniforms.get());
			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			bgfx::setState(BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_DEPTH_WRITE/* | BGFX_STATE_CULL_CW*/);
			bgfx::submit(viewId, s_main->shaderPrograms[ShaderProgramId::Depth].handle);
			s_main->currentEntity = nullptr;
		}

		s_main->uniforms->lightModelViewProj.set(shadowProjectionMatrix * shadowViewMatrix);
		s_main->uniforms->shadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias.set(vec4(1.0f / s_main->shadowMapSize, g_cvars.shadowDepthBias.getFloat(), g_cvars.shadowNormalBias.getFloat(), g_cvars.shadowSlopeScaleDepthBias.getFloat()));
		s_main->uniforms->sunLightColor.set(vec4(s_main->sunLight.light * g_cvars.sunLightIntensity.getFloat(), 0));
		s_main->uniforms->sunLightDir.set(vec4(-s_main->sunLight.direction, 0));
	}

	// Render depth for soft sprites. MSAA is always off.
	if (s_main->softSpritesEnabled && s_main->isWorldCamera && !isProbe)
	{
		const bgfx::ViewId viewId = PushView(s_main->depthFb, BGFX_CLEAR_DEPTH, viewMatrix, projectionMatrix, args.rect);
#ifdef _DEBUG
		bgfx::setViewName(viewId, "Depth");
#endif

		for (DrawCall &dc : s_main->drawCalls)
		{
			// Material remapping.
			Material *mat = dc.material->remappedShader ? dc.material->remappedShader : dc.material;

			if (mat->sort != MaterialSort::Opaque || mat->numUnfoggedPasses == 0)
				continue;

			// Don't render reflective geometry with the reflection camera.
			if (args.visId == VisibilityId::Reflection && mat->reflective != MaterialReflective::None)
				continue;

			s_main->currentEntity = dc.entity;
			s_main->matUniforms->time.set(vec4(mat->setTime(s_main->floatTime), 0, 0, 0));

			if (dc.zOffset > 0 || dc.zScale > 0)
			{
				s_main->uniforms->depthRangeEnabled.set(vec4(1, 0, 0, 0));
				s_main->uniforms->depthRange.set(vec4(dc.zOffset, dc.zScale, depthRange.x, depthRange.y));
			}
			else
			{
				s_main->uniforms->depthRangeEnabled.set(vec4::empty);
			}

			mat->setDeformUniforms(s_main->matUniforms.get());

			// See if any of the stages use alpha testing.
			const MaterialStage *alphaTestStage = nullptr;

			for (const MaterialStage &stage : mat->stages)
			{
				if (stage.active && stage.alphaTest != MaterialAlphaTest::None)
				{
					alphaTestStage = &stage;
					break;
				}
			}

			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			uint64_t state = BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_DEPTH_WRITE;

			// Grab the cull state. Doesn't matter which stage, since it's global to the material.
			state |= mat->stages[0].getState() & BGFX_STATE_CULL_MASK;

			int shaderVariant = DepthShaderProgramVariant::None;

			if (alphaTestStage)
			{
				alphaTestStage->setShaderUniforms(s_main->matStageUniforms.get(), MaterialStageSetUniformsFlags::TexGen);
				bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, alphaTestStage->bundles[0].textures[0]->getHandle());
				shaderVariant |= DepthShaderProgramVariant::AlphaTest;
			}
			else
			{
				s_main->matStageUniforms->alphaTest.set(vec4::empty);
			}

			bgfx::setState(state);

			if (args.flags & RenderCameraFlags::UseStencilTest)
			{
				bgfx::setStencil(stencilTest);
			}

			bgfx::submit(viewId, s_main->shaderPrograms[ShaderProgramId::Depth + shaderVariant].handle);
			s_main->currentEntity = nullptr;
		}
	}

	bgfx::ViewId mainViewId;
	
	if (isProbe)
	{
		assert(bgfx::isValid(args.customFrameBuffer->handle));
		mainViewId = PushView(*args.customFrameBuffer, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, viewMatrix, projectionMatrix, args.rect, PushViewFlags::Sequential);
#ifdef _DEBUG
		bgfx::setViewName(mainViewId, "Probe");
#endif
	}
	else if (s_main->isWorldCamera)
	{
		mainViewId = PushView(s_main->fastPathEnabled ? s_main->defaultFb : s_main->sceneFb, BGFX_CLEAR_DEPTH, viewMatrix, projectionMatrix, args.rect, PushViewFlags::Sequential);
#ifdef _DEBUG
		bgfx::setViewName(mainViewId, "Scene");
#endif
	}
	else
	{
		mainViewId = PushView(s_main->defaultFb, BGFX_CLEAR_DEPTH, viewMatrix, projectionMatrix, args.rect, PushViewFlags::Sequential);
#ifdef _DEBUG
		bgfx::setViewName(mainViewId, "HudScene");
#endif
	}

	if (!s_main->drawCalls.empty())
	{
		int renderMode = RENDER_MODE_NONE;

		if (args.flags & RenderCameraFlags::SkipUnlitSurfaces)
			renderMode = RENDER_MODE_LIT;
		else if (!isProbe && g_cvars.debug.getInt() == 1)
			renderMode = RENDER_MODE_LIGHTMAP;

		s_main->uniforms->renderMode.set(vec4((float)renderMode, 0, 0, 0));
	}

	for (DrawCall &dc : s_main->drawCalls)
	{
		assert(dc.material);

		// Material remapping.
		Material *mat = dc.material->remappedShader ? dc.material->remappedShader : dc.material;

		// Don't render reflective geometry with the reflection camera.
		if (args.visId == VisibilityId::Reflection && mat->reflective != MaterialReflective::None)
			continue;

		// Special case for skybox.
		if (dc.flags & DrawCallFlags::Skybox)
		{
			if (s_main->bloomEnabled)
			{
				s_main->uniforms->bloom_Enabled_Write_Scale.set(vec4(1, 0, 0, 0));
			}
			else
			{
				s_main->uniforms->bloom_Enabled_Write_Scale.set(vec4::empty);
			}

			s_main->uniforms->depthRangeEnabled.set(vec4(1, 0, 0, 0));
			s_main->uniforms->depthRange.set(vec4(dc.zOffset, dc.zScale, depthRange.x, depthRange.y));
			s_main->uniforms->dynamicLight_Num_Intensity.set(vec4::empty);
			s_main->matUniforms->nDeforms.set(vec4(0, 0, 0, 0));
			s_main->matStageUniforms->alphaTest.set(vec4::empty);
			s_main->matStageUniforms->baseColor.set(vec4::white);
			s_main->matStageUniforms->generators.set(vec4::empty);
			s_main->matStageUniforms->lightType.set(vec4::empty);
			s_main->matStageUniforms->vertexColor.set(vec4::black);
			const int sky_texorder[6] = { 0, 2, 1, 3, 4, 5 };
			bgfx::setTexture(TextureUnit::Diffuse, s_main->matStageUniforms->diffuseSampler.handle, mat->sky.outerbox[sky_texorder[dc.skyboxSide]]->getHandle());
#ifdef _DEBUG
			bgfx::setTexture(TextureUnit::Diffuse2, s_main->matStageUniforms->diffuseSampler2.handle, g_textureCache->getWhite()->getHandle());
			bgfx::setTexture(TextureUnit::Light, s_main->matStageUniforms->lightSampler.handle, g_textureCache->getWhite()->getHandle());
#endif
			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			uint64_t state = dc.state;

			if (IsMsaa(s_main->aa))
				state |= BGFX_STATE_MSAA;

			bgfx::setState(state);

			if (args.flags & RenderCameraFlags::UseStencilTest)
			{
				bgfx::setStencil(stencilTest);
			}

			bgfx::submit(mainViewId, s_main->shaderPrograms[ShaderProgramId::Generic].handle);
			continue;
		}

		const bool doFogPass = !dc.material->noFog && dc.fogIndex >= 0 && mat->fogPass != MaterialFogPass::None;

		if (mat->numUnfoggedPasses == 0 && !doFogPass)
			continue;

		s_main->currentEntity = dc.entity;
		s_main->matUniforms->time.set(vec4(mat->setTime(s_main->floatTime), 0, 0, 0));
		const mat4 modelViewMatrix(viewMatrix * dc.modelMatrix);

		if (s_main->isWorldCamera)
		{
			s_main->dlightManager->updateUniforms(s_main->uniforms.get());
		}
		else
		{
			// For non-world scenes, dlight contribution is added to entities in SetupEntityLighting, so write 0 to the uniform for num dlights.
			s_main->uniforms->dynamicLight_Num_Intensity.set(vec4::empty);
		}

		if (mat->polygonOffset)
		{
			s_main->uniforms->depthRange.set(vec4(polygonDepthOffset, 1, depthRange.x, depthRange.y));
		}
		else
		{
			s_main->uniforms->depthRange.set(vec4(dc.zOffset, dc.zScale, depthRange.x, depthRange.y));
		}

		s_main->uniforms->viewOrigin.set(args.position);
		s_main->uniforms->viewUp.set(args.rotation[2]);
		mat->setDeformUniforms(s_main->matUniforms.get());
		const vec3 localViewPosition = s_main->currentEntity ? s_main->currentEntity->localViewPosition : args.position;
		s_main->uniforms->localViewOrigin.set(localViewPosition);

		if (s_main->currentEntity)
		{
			s_main->entityUniforms->ambientLight.set(vec4(util::ToLinear(s_main->currentEntity->ambientLight / 255.0f), 0));
			s_main->entityUniforms->directedLight.set(vec4(util::ToLinear(s_main->currentEntity->directedLight / 255.0f), 0));
			s_main->entityUniforms->lightDirection.set(vec4(s_main->currentEntity->lightDir, 0));
		}

		vec4 fogColor, fogDistance, fogDepth;
		float eyeT;

		if (!dc.material->noFog && dc.fogIndex >= 0)
		{
			world::CalculateFog(dc.fogIndex, dc.modelMatrix, modelViewMatrix, args.position, localViewPosition, args.rotation, &fogColor, &fogDistance, &fogDepth, &eyeT);
			s_main->uniforms->fogDistance.set(fogDistance);
			s_main->uniforms->fogDepth.set(fogDepth);
			s_main->uniforms->fogEyeT.set(eyeT);
		}

		for (const MaterialStage &stage : mat->stages)
		{
			if (!stage.active)
				continue;

			if (s_main->bloomEnabled)
			{
				s_main->uniforms->bloom_Enabled_Write_Scale.set(vec4(1, stage.bloom ? 1.0f : 0.0f, 0, 0));
			}
			else
			{
				s_main->uniforms->bloom_Enabled_Write_Scale.set(vec4::empty);
			}

			if (mat->polygonOffset || dc.zOffset > 0 || dc.zScale > 0)
			{
				s_main->uniforms->depthRangeEnabled.set(vec4(1, 0, 0, 0));
			}
			else
			{
				s_main->uniforms->depthRangeEnabled.set(vec4::empty);
			}

			if (!dc.material->noFog && dc.fogIndex >= 0 && stage.adjustColorsForFog != MaterialAdjustColorsForFog::None)
			{
				s_main->uniforms->fogEnabled.set(vec4(1, 0, 0, 0));
				s_main->matStageUniforms->fogColorMask.set(stage.getFogColorMask());
			}
			else
			{
				s_main->uniforms->fogEnabled.set(vec4::empty);
			}

			stage.setShaderUniforms(s_main->matStageUniforms.get());
			stage.setTextureSamplers(s_main->matStageUniforms.get());
			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			uint64_t state = dc.state | stage.getState();

			if (IsMsaa(s_main->aa))
				state |= BGFX_STATE_MSAA;

			int shaderVariant = GenericShaderProgramVariant::None;

			if (stage.alphaTest != MaterialAlphaTest::None)
			{
				shaderVariant |= GenericShaderProgramVariant::AlphaTest;
			}
			else if (s_main->isWorldCamera && s_main->softSpritesEnabled && dc.softSpriteDepth > 0)
			{
				shaderVariant |= GenericShaderProgramVariant::SoftSprite;
				bgfx::setTexture(TextureUnit::Depth, s_main->matStageUniforms->depthSampler.handle, bgfx::getTexture(s_main->depthFb.handle));
				
				// Change additive blend from (1, 1) to (src alpha, 1) so the soft sprite shader can control alpha.
				float useAlpha = 1;

				if ((state & BGFX_STATE_BLEND_MASK) == BGFX_STATE_BLEND_ADD)
				{
					useAlpha = 0; // Ignore existing alpha values in the shader. This preserves the behavior of a (1, 1) additive blend.
					state &= ~BGFX_STATE_BLEND_MASK;
					state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
				}

				s_main->uniforms->softSprite_Depth_UseAlpha.set(vec4(dc.softSpriteDepth, useAlpha, 0, 0));
			}

			if (s_main->isWorldCamera && dc.dynamicLighting && !(dc.flags & DrawCallFlags::Sky))
			{
				shaderVariant |= GenericShaderProgramVariant::DynamicLights;
				bgfx::setTexture(TextureUnit::DynamicLightCells, s_main->matStageUniforms->dynamicLightCellsSampler.handle, s_main->dlightManager->getCellsTexture());
				bgfx::setTexture(TextureUnit::DynamicLightIndices, s_main->matStageUniforms->dynamicLightIndicesSampler.handle, s_main->dlightManager->getIndicesTexture());
				bgfx::setTexture(TextureUnit::DynamicLights, s_main->matStageUniforms->dynamicLightsSampler.handle, s_main->dlightManager->getLightsTexture());
			}

			if (s_main->sunLightEnabled && s_main->isWorldCamera && mat->sort == MaterialSort::Opaque && !(dc.flags & DrawCallFlags::Sky))
			{
				shaderVariant |= GenericShaderProgramVariant::SunLight;
				bgfx::setTexture(TextureUnit::ShadowMap, s_main->uniforms->shadowMapSampler.handle, bgfx::getTexture(s_main->shadowMapFb.handle));
			}

			bgfx::setState(state);

			if (args.flags & RenderCameraFlags::UseStencilTest)
			{
				bgfx::setStencil(stencilTest);
			}

			if (!s_main->fastPathEnabled && g_cvars.textureVariation.getBool() && stage.textureVariation)
			{
				if (shaderVariant & GenericShaderProgramVariant::SunLight)
				{
					shaderVariant = TextureVariationShaderProgramVariant::SunLight;
				}
				else
				{
					shaderVariant = 0;
				}

				//bgfx::setTexture(TextureUnit::Noise, s_main->uniforms->noiseSampler.handle, g_textureCache->getNoise()->getHandle());
				bgfx::submit(mainViewId, s_main->shaderPrograms[ShaderProgramId::TextureVariation + shaderVariant].handle);
			}
			else
			{
				bgfx::submit(mainViewId, s_main->shaderPrograms[ShaderProgramId::Generic + shaderVariant].handle);
			}
		}

		if (g_cvars.wireframe.getBool())
		{
			// Doesn't handle vertex deforms.
			s_main->matStageUniforms->color.set(vec4::white);
			SetDrawCallGeometry(dc);
			bgfx::setState(dc.state | BGFX_STATE_DEPTH_TEST_ALWAYS | BGFX_STATE_PT_LINES);
			bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, g_textureCache->getWhite()->getHandle());
			bgfx::setTransform(dc.modelMatrix.get());
			bgfx::submit(mainViewId, s_main->shaderPrograms[ShaderProgramId::TextureColor].handle);
		}

		// Do fog pass.
		if (doFogPass)
		{
			if (s_main->bloomEnabled)
			{
				s_main->uniforms->bloom_Enabled_Write_Scale.set(vec4(1, 0, 0, 0));
			}
			else
			{
				s_main->uniforms->bloom_Enabled_Write_Scale.set(vec4::empty);
			}

			if (dc.zOffset > 0 || dc.zScale > 0)
			{
				s_main->uniforms->depthRangeEnabled.set(vec4(1, 0, 0, 0));
			}
			else
			{
				s_main->uniforms->depthRangeEnabled.set(vec4::empty);
			}

			s_main->matStageUniforms->color.set(fogColor);
			SetDrawCallGeometry(dc);
			bgfx::setTransform(dc.modelMatrix.get());
			uint64_t state = dc.state | BGFX_STATE_BLEND_ALPHA;

			if (IsMsaa(s_main->aa))
				state |= BGFX_STATE_MSAA;

			if (mat->fogPass == MaterialFogPass::Equal)
			{
				state |= BGFX_STATE_DEPTH_TEST_EQUAL;
			}
			else
			{
				state |= BGFX_STATE_DEPTH_TEST_LEQUAL;
			}

			bgfx::setState(state);

			if (args.flags & RenderCameraFlags::UseStencilTest)
			{
				bgfx::setStencil(stencilTest);
			}

			bgfx::submit(mainViewId, s_main->shaderPrograms[ShaderProgramId::Fog].handle);
		}

		s_main->currentEntity = nullptr;
	}

	// Draws x/y/z lines from the origin for orientation debugging
	if (!s_main->sceneDebugAxis.empty())
	{
		bgfx::TransientVertexBuffer tvb;
		bgfx::allocTransientVertexBuffer(&tvb, 6, Vertex::decl);
		auto vertices = (Vertex *)tvb.data;
		const float l = 16;
		vertices[0].pos = { 0, 0, 0 }; vertices[0].setColor(vec4::red);
		vertices[1].pos = { l, 0, 0 }; vertices[1].setColor(vec4::red);
		vertices[2].pos = { 0, 0, 0 }; vertices[2].setColor(vec4::green);
		vertices[3].pos = { 0, l, 0 }; vertices[3].setColor(vec4::green);
		vertices[4].pos = { 0, 0, 0 }; vertices[4].setColor(vec4::blue);
		vertices[5].pos = { 0, 0, l }; vertices[5].setColor(vec4::blue);

		for (vec3 pos : s_main->sceneDebugAxis)
		{
			bgfx::setState(BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_PT_LINES | BGFX_STATE_RGB_WRITE);
			bgfx::setTransform(mat4::translate(pos).get());
			bgfx::setVertexBuffer(0, &tvb);
			bgfx::submit(mainViewId, s_main->shaderPrograms[ShaderProgramId::Color].handle);
		}
	}

	// Debug draw bounds.
	if (!s_main->sceneDebugBounds.empty())
	{
		const uint32_t nVertices = 24;
		const vec4 randomColors[] =
		{
			{ 1, 0, 0, 1 },
			{ 0, 1, 0, 1 },
			{ 0, 0, 1, 1 },
			{ 1, 1, 0, 1 },
			{ 0, 1, 1, 1 },
			{ 1, 0, 1, 1 }
		};

		bgfx::TransientVertexBuffer tvb;
		bgfx::allocTransientVertexBuffer(&tvb, nVertices * (uint32_t)s_main->sceneDebugBounds.size(), Vertex::decl);
		auto v = (Vertex *)tvb.data;

		for (size_t i = 0; i < s_main->sceneDebugBounds.size(); i++)
		{
			const std::array<vec3, 8> corners = s_main->sceneDebugBounds[i].toVertices();

			for (int j = 0; j < nVertices; j++)
				v[j].setColor(randomColors[i % BX_COUNTOF(randomColors)]);

			// Top.
			v[0].pos = corners[0]; v[1].pos = corners[1];
			v[2].pos = corners[1]; v[3].pos = corners[2];
			v[4].pos = corners[2]; v[5].pos = corners[3];
			v[6].pos = corners[3]; v[7].pos = corners[0];
			v += 8;

			// Bottom.
			v[0].pos = corners[4]; v[1].pos = corners[5];
			v[2].pos = corners[5]; v[3].pos = corners[6];
			v[4].pos = corners[6]; v[5].pos = corners[7];
			v[6].pos = corners[7]; v[7].pos = corners[4];
			v += 8;

			// Connect bottom and top.
			v[0].pos = corners[0]; v[1].pos = corners[4];
			v[2].pos = corners[1]; v[3].pos = corners[7];
			v[4].pos = corners[2]; v[5].pos = corners[6];
			v[6].pos = corners[3]; v[7].pos = corners[5];
			v += 8;
		}

		bgfx::setState(BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_PT_LINES | BGFX_STATE_RGB_WRITE);
		bgfx::setVertexBuffer(0, &tvb);
		bgfx::submit(mainViewId, s_main->shaderPrograms[ShaderProgramId::Color].handle);
	}
}

void RenderScene(const SceneDefinition &scene)
{
	FlushStretchPics();
	s_main->stretchPicViewId = UINT8_MAX;
	s_main->time = scene.time;
	s_main->floatTime = s_main->time * 0.001f;
	
	// Clamp view rect to screen.
	Rect rect;
	rect.x = std::max(0, scene.rect.x);
	rect.y = std::max(0, scene.rect.y);
#if 0
	rect.w = std::min(window::GetWidth(), rect.x + scene.rect.w) - rect.x;
	rect.h = std::min(window::GetHeight(), rect.y + scene.rect.h) - rect.y;
#else
	rect.w = scene.rect.w;
	rect.h = scene.rect.h;
#endif

	if (scene.flags & SceneDefinitionFlags::Hyperspace)
	{
		const uint8_t c = s_main->time & 255;
		const bgfx::ViewId viewId = PushView(s_main->defaultFb, 0, mat4::identity, mat4::identity, rect);
		bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, (c<<24)|(c<<16)|(c<<8)|0xff);
		bgfx::touch(viewId);
	}
	else if (scene.flags & SceneDefinitionFlags::SkyboxPortal)
	{
		// Render the skybox portal as a camera in the containing scene.
		s_main->skyboxPortalEnabled = true;
		s_main->skyboxPortalScene = scene;
	}
	else
	{
		const bool isWorldScene = (scene.flags & SceneDefinitionFlags::World) != 0;
		assert(!isWorldScene || (isWorldScene && world::IsLoaded()));

		// Need to do this here because AddEntityToScene doesn't know if this is a world scene.
		for (const Entity &entity : s_main->sceneEntities)
		{
			meta::OnEntityAddedToScene(entity, isWorldScene);
		}

		// Update scene dynamic lights.
		if (isWorldScene)
		{
			s_main->dlightManager->updateTextures(s_main->frameNo);
		}

		// Render camera(s).
		s_main->sceneRotation = scene.rotation;

		if (s_main->skyboxPortalEnabled)
		{
			RenderCameraArgs args;
			args.areaMask = s_main->skyboxPortalScene.areaMask;
			args.flags = RenderCameraFlags::IsSkyboxPortal;
			args.fov = s_main->skyboxPortalScene.fov;
			args.position = s_main->skyboxPortalScene.position;
			args.pvsPosition = s_main->skyboxPortalScene.position;
			args.rect = rect;
			args.rotation = s_main->skyboxPortalScene.rotation;
			args.visId = VisibilityId::SkyboxPortal;
			RenderCamera(args);
			s_main->skyboxPortalEnabled = false;
		}

		RenderCameraArgs args;
		args.areaMask = scene.areaMask;
		args.fov = scene.fov;
		args.position = scene.position;
		args.pvsPosition = scene.position;
		args.rect = rect;
		args.rotation = s_main->sceneRotation;
		args.visId = isWorldScene ? VisibilityId::Main : VisibilityId::None;

		if (scene.flags & SceneDefinitionFlags::ContainsSkyboxPortal)
			args.flags |= RenderCameraFlags::ContainsSkyboxPortal;

		RenderCamera(args);

		if (isWorldScene)
		{
			if (s_main->bloomEnabled)
			{
				// OpenGL resolves multisampled bloom into a temp texture.
				const bool msaaResolve = bgfx::getRendererType() == bgfx::RendererType::OpenGL && IsMsaa(s_main->aa);

				if (msaaResolve)
				{
					Blit("BloomMsaaResolve", bgfx::getTexture(s_main->sceneFb.handle, s_main->sceneBloomAttachment), bgfx::getTexture(s_main->sceneTempFb.handle));
				}

				// Render to quarter size framebuffer.
				const Rect bloomRect(0, 0, window::GetWidth() / 4, window::GetHeight() / 4);
				bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, msaaResolve ? bgfx::getTexture(s_main->sceneTempFb.handle) : bgfx::getTexture(s_main->sceneFb.handle, s_main->sceneBloomAttachment));
				RenderScreenSpaceQuad("BloomCopy", s_main->bloomFb[0], ShaderProgramId::Texture, BGFX_STATE_RGB_WRITE, BGFX_CLEAR_NONE, s_main->isTextureOriginBottomLeft, bloomRect);

				// Ping-pong guassian blur in quarter size framebuffers
				for (int i = 0; i < 2; i++)
				{
					s_main->uniforms->guassianBlurDirection.set(i == 0 ? vec4(1, 0, 0, 0) : vec4(0, 1, 0, 0));
					bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, bgfx::getTexture(s_main->bloomFb[i].handle));
					RenderScreenSpaceQuad("BloomBlur", s_main->bloomFb[!i], ShaderProgramId::GaussianBlur, BGFX_STATE_RGB_WRITE, BGFX_CLEAR_NONE, s_main->isTextureOriginBottomLeft, bloomRect);
				}

				// Apply bloom. If using SMAA, we need to read color, so blit into the original bloom texture which is no longer used.
				s_main->uniforms->bloom_Enabled_Write_Scale.set(vec4(1, 0, g_cvars.bloomScale.getFloat(), 0));
				bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, bgfx::getTexture(s_main->sceneFb.handle));
				bgfx::setTexture(1, s_main->uniforms->bloomSampler.handle, bgfx::getTexture(s_main->bloomFb[0].handle));
				RenderScreenSpaceQuad("BloomApply", s_main->aa == AntiAliasing::SMAA ? s_main->sceneTempFb : s_main->defaultFb, ShaderProgramId::Bloom, BGFX_STATE_RGB_WRITE, BGFX_CLEAR_NONE, s_main->isTextureOriginBottomLeft);
			}

			if (s_main->aa == AntiAliasing::SMAA)
			{
				s_main->uniforms->smaaMetrics.set(vec4(1.0f / rect.w, 1.0f / rect.h, (float)rect.w, (float)rect.h));

				// Edge detection.
				if (s_main->bloomEnabled)
				{
					bgfx::setTexture(0, s_main->uniforms->smaaColorSampler.handle, bgfx::getTexture(s_main->sceneTempFb.handle));
				}
				else
				{
					bgfx::setTexture(0, s_main->uniforms->smaaColorSampler.handle, bgfx::getTexture(s_main->sceneFb.handle));
				}

				RenderScreenSpaceQuad("SMAAEdgeDetection", s_main->smaaEdgesFb, ShaderProgramId::SMAAEdgeDetection, BGFX_STATE_RGB_WRITE, BGFX_CLEAR_COLOR, s_main->isTextureOriginBottomLeft);

				// Blending weight calculation.
				bgfx::setTexture(0, s_main->uniforms->smaaEdgesSampler.handle, bgfx::getTexture(s_main->smaaEdgesFb.handle));
				bgfx::setTexture(1, s_main->uniforms->smaaAreaSampler.handle, s_main->smaaAreaTex);
				bgfx::setTexture(2, s_main->uniforms->smaaSearchSampler.handle, s_main->smaaSearchTex);
				RenderScreenSpaceQuad("SMAABlendingWeightCalculation", s_main->smaaBlendFb, ShaderProgramId::SMAABlendingWeightCalculation, BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE, BGFX_CLEAR_COLOR, s_main->isTextureOriginBottomLeft);

				// Neighborhood blending.
				if (s_main->bloomEnabled)
				{
					bgfx::setTexture(0, s_main->uniforms->smaaColorSampler.handle, bgfx::getTexture(s_main->sceneTempFb.handle));
				}
				else
				{
					bgfx::setTexture(0, s_main->uniforms->smaaColorSampler.handle, bgfx::getTexture(s_main->sceneFb.handle));
				}

				bgfx::setTexture(1, s_main->uniforms->smaaBlendSampler.handle, bgfx::getTexture(s_main->smaaBlendFb.handle));
				RenderScreenSpaceQuad("SMAANeighborhoodBlending", s_main->defaultFb, ShaderProgramId::SMAANeighborhoodBlending, BGFX_STATE_RGB_WRITE, BGFX_CLEAR_NONE, s_main->isTextureOriginBottomLeft);
			}
			else if (!s_main->bloomEnabled && !s_main->fastPathEnabled)
			{
				// Render scene to backbuffer.
				bgfx::setTexture(0, s_main->uniforms->textureSampler.handle, bgfx::getTexture(s_main->sceneFb.handle));
				RenderScreenSpaceQuad("RenderToBackbuffer", s_main->defaultFb, ShaderProgramId::Texture, BGFX_STATE_RGB_WRITE, BGFX_CLEAR_NONE, s_main->isTextureOriginBottomLeft);
			}
		}
	}

	s_main->dlightManager->clear();
	s_main->sceneDebugAxis.clear();
	s_main->sceneDebugBounds.clear();
	s_main->sceneEntities.clear();
	s_main->scenePolygons.clear();
	s_main->sortedScenePolygons.clear();
	s_main->scenePolygonVertices.clear();
}

/***********************************************************
* A single header file OpenGL lightmapping library         *
* https://github.com/ands/lightmapper                      *
* no warranty implied | use at your own risk               *
* author: Andreas Mantler (ands) | last change: 23.07.2016 *
*                                                          *
* License:                                                 *
* This software is in the public domain.                   *
* Where that dedication is not recognized,                 *
* you are granted a perpetual, irrevocable license to copy *
* and modify this file however you want.                   *
***********************************************************/
void RenderHemicube(const FrameBuffer &frameBuffer, vec3 position, const vec3 forward, const vec3 up, vec2i rectOffset, int faceSize, bool skipUnlitSurfaces)
{
	// +-------+---+---+-------+
	// |       |   |   |   D   |
	// |   C   | R | L +-------+
	// |       |   |   |   U   |
	// +-------+---+---+-------+
	// Order: C,R,L,D,U
	const Rect rects[] =
	{
		Rect(rectOffset.x, rectOffset.y, faceSize, faceSize),
		Rect(rectOffset.x + faceSize, rectOffset.y, faceSize / 2, faceSize),
		Rect(rectOffset.x + faceSize + faceSize / 2, rectOffset.y, faceSize / 2, faceSize),
		Rect(rectOffset.x + faceSize * 2, rectOffset.y, faceSize, faceSize / 2),
		Rect(rectOffset.x + faceSize * 2, rectOffset.y + faceSize / 2, faceSize, faceSize / 2)
	};

	const vec3 right = vec3::crossProduct(forward, up);

	const mat3 rotations[] =
	{
		mat3(forward, -right, up),
		mat3(right, -vec3::crossProduct(right, up), up),
		mat3(-right, -vec3::crossProduct(-right, up), up),
		mat3(-up, -vec3::crossProduct(-up, forward), forward),
		mat3(up, -vec3::crossProduct(up, -forward), -forward),
	};

	world::UpdateVisibility(VisibilityId::Probe, position, nullptr);
	const vec2 depthRange = CalculateDepthRange(VisibilityId::Probe, position);
	const float zNear = depthRange.x;
	const float zFar = depthRange.y;

	const mat4 projectionMatrices[] =
	{
		mat4::perspectiveProjection(-zNear, zNear, zNear, -zNear, zNear, zFar),
		mat4::perspectiveProjection(-zNear, 0.0f, zNear, -zNear, zNear, zFar),
		mat4::perspectiveProjection(0.0f, zNear, zNear, -zNear, zNear, zFar),
		mat4::perspectiveProjection(-zNear, zNear, zNear, 0.0f, zNear, zFar),
		mat4::perspectiveProjection(-zNear, zNear, 0.0f, -zNear, zNear, zFar)
	};

	for (int i = 0; i < 5; i++)
	{
		s_main->sceneRotation = rotations[i];

		RenderCameraArgs args;
		args.customFrameBuffer = &frameBuffer;
		args.customProjectionMatrix = &projectionMatrices[i];
		args.flags = skipUnlitSurfaces ? RenderCameraFlags::SkipUnlitSurfaces : 0;
		args.position = position;
		args.pvsPosition = position;
		args.rect = rects[i];
		args.rotation = rotations[i];
		args.visId = VisibilityId::Probe;
		RenderCamera(args);
	}
}

void EndFrame()
{
	FlushStretchPics();

#if defined(USE_LIGHT_BAKER)
	light_baker::Update(s_main->frameNo);
#endif

	if (s_main->firstFreeViewId == 0)
	{
		// No active views. Make sure the screen is cleared.
		const bgfx::ViewId viewId = PushView(s_main->defaultFb, 0, mat4::identity, mat4::identity, Rect(0, 0, window::GetWidth(), window::GetHeight()));
		bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x000000ff);
		bgfx::touch(viewId);
	}

	if (s_main->debugDraw == DebugDraw::Bloom && s_main->bloomEnabled)
	{
		if (bgfx::getRendererType() == bgfx::RendererType::OpenGL && IsMsaa(s_main->aa))
		{
			RenderDebugDraw(bgfx::getTexture(s_main->sceneTempFb.handle));
		}
		else
		{
			RenderDebugDraw(bgfx::getTexture(s_main->sceneFb.handle, s_main->sceneBloomAttachment));
		}

		RenderDebugDraw(bgfx::getTexture(s_main->bloomFb[0].handle), 0, 1);
		RenderDebugDraw(bgfx::getTexture(s_main->bloomFb[1].handle), 0, 2);
	}
	else if (s_main->debugDraw == DebugDraw::Depth && s_main->softSpritesEnabled)
	{
		s_main->uniforms->depthRange.set(vec4(0, 0, s_main->lastCameraDepthRange.x, s_main->lastCameraDepthRange.y));
		s_main->uniforms->textureDebug.set(vec4(TEXTURE_DEBUG_LINEAR_DEPTH, 0, 0, 0));
		RenderDebugDraw(bgfx::getTexture(s_main->depthFb.handle), 0, 0, ShaderProgramId::TextureDebug);
	}
	else if (s_main->debugDraw == DebugDraw::DynamicLight)
	{
		s_main->uniforms->textureDebug.set(vec4(TEXTURE_DEBUG_R, 0, 0, 0));
		RenderDebugDraw(s_main->dlightManager->getLightsTexture(), 0, 0, ShaderProgramId::TextureDebug);
	}
	else if (s_main->debugDraw == DebugDraw::Lightmap && world::IsLoaded())
	{
		for (int i = 0; i < world::GetNumLightmaps(); i++)
		{
			RenderDebugDraw(world::GetLightmap(i)->getHandle(), i, 0, ShaderProgramId::Texture);
		}
	}
	else if (s_main->debugDraw == DebugDraw::Reflection && s_main->waterReflectionsEnabled)
	{
		RenderDebugDraw(bgfx::getTexture(s_main->reflectionFb.handle));
	}
	else if (s_main->debugDraw == DebugDraw::SMAA && s_main->aa == AntiAliasing::SMAA)
	{
		s_main->uniforms->textureDebug.set(vec4(TEXTURE_DEBUG_R, 0, 0, 0));
		RenderDebugDraw(bgfx::getTexture(s_main->smaaEdgesFb.handle), 0, 0, ShaderProgramId::TextureDebug);
		RenderDebugDraw(bgfx::getTexture(s_main->smaaBlendFb.handle), 1, 0, ShaderProgramId::TextureDebug);
	}
	else if (s_main->debugDraw == DebugDraw::Shadow && s_main->sunLightEnabled)
	{
		s_main->uniforms->textureDebug.set(vec4(TEXTURE_DEBUG_R, 0, 0, 0));
		RenderDebugDraw(bgfx::getTexture(s_main->shadowMapFb.handle), 0, 0, ShaderProgramId::TextureDebug);
	}

#ifdef USE_PROFILER
	PROFILE_END // Frame
	profiler::Print();
	profiler::BeginFrame(s_main->frameNo + 1);
	PROFILE_BEGIN(Frame)
#endif

	uint32_t debug = 0;

	if (g_cvars.bgfx_stats.getBool())
		debug |= BGFX_DEBUG_STATS;

	if (s_main->debugTextThisFrame)
		debug |= BGFX_DEBUG_TEXT;

	bgfx::setDebug(debug);
	s_main->frameNo = bgfx::frame(s_main->captureFrame);
	s_main->captureFrame = false;

	if (g_cvars.debugDraw.isModified())
	{
		s_main->debugDraw = DebugDrawFromString(g_cvars.debugDraw.getString());
		g_cvars.debugDraw.clearModified();
	}

	if (g_cvars.gamma.isModified())
	{
		SetWindowGamma();
		g_cvars.gamma.clearModified();
	}

	if (s_main->debugTextThisFrame)
	{
		bgfx::dbgTextClear();
		s_main->debugTextThisFrame = false;
		s_main->debugTextY = 0;
	}

	s_main->firstFreeViewId = 0;
	s_main->stretchPicViewId = UINT8_MAX;
}

} // namespace main
} // namespace renderer
