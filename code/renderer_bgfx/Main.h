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
#pragma once

#include "Precompiled.h"
#pragma hdrstop

namespace renderer {

#include "../../build/Shader.h" // Pull into the renderer namespace.

namespace main {

enum class AntiAliasing
{
	None,
	MSAA2x,
	MSAA4x,
	MSAA8x,
	MSAA16x,
	SMAA
};

struct BgfxCallback : bgfx::CallbackI
{
	void fatal(bgfx::Fatal::Enum _code, const char* _str) override;
	void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override;
	void profilerBegin(const char* _name, uint32_t _abgr, const char* _filePath, uint16_t _line) override {};
	void profilerBeginLiteral(const char* _name, uint32_t _abgr, const char* _filePath, uint16_t _line) override {};
	void profilerEnd() override {};
	uint32_t cacheReadSize(uint64_t _id) override { return 0; }
	bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override { return false; }
	void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override {};
	void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t _size, bool _yflip) override;
	void captureBegin(uint32_t _width, uint32_t _height, uint32_t _pitch, bgfx::TextureFormat::Enum _format, bool _yflip) override {};
	void captureEnd() override {};
	void captureFrame(const void* _data, uint32_t _size) override {};

private:
	std::vector<uint8_t> screenShotDataBuffer_;
	std::vector<uint8_t> screenShotFileBuffer_;
};

enum class DebugDraw
{
	None,
	Bloom,
	Depth,
	DynamicLight,
	Lightmap,
	Reflection,
	Shadow,
	SMAA
};

struct DepthShaderProgramVariant
{
	enum
	{
		None       = 0,
		AlphaTest  = 1 << 0,
		Num        = 1 << 1
	};
};

/// @remarks Sync with generated GenericFragmentShaderVariant and GenericVertexShaderVariant. Order matters - fragment first.
struct GenericShaderProgramVariant
{
	enum
	{
		None = 0,

		// Fragment
		AlphaTest = 1 << 0,
		DynamicLights = 1 << 1,
		SoftSprite = 1 << 2,
		SunLight = 1 << 3,

		Num = 1 << 4
	};
};

struct TextureVariationShaderProgramVariant
{
	enum
	{
		None     = 0,
		SunLight = 1 << 0,
		Num      = 1 << 1
	};
};

struct ShaderProgramId
{
	enum Enum
	{
		Bloom,
		Color,
		Depth,
		Fog = Depth + DepthShaderProgramVariant::Num,
		GaussianBlur,
		Generic,
		HemicubeDownsample = Generic + GenericShaderProgramVariant::Num,
		HemicubeWeightedDownsample,
		SMAABlendingWeightCalculation,
		SMAAEdgeDetection,
		SMAANeighborhoodBlending,
		Texture,
		TextureColor,
		TextureDebug,
		TextureVariation,
		Num = TextureVariation + TextureVariationShaderProgramVariant::Num
	};
};

struct Main
{
	/// @name Camera
	/// @{
	DrawCallList drawCalls;

	/// Flip face culling if true.
	bool isCameraMirrored = false;

	/// Does the current camera render the world - i.e. not part of a HUD/UI scene.
	bool isWorldCamera = false;

	/// @}

	/// @name Fonts
	/// @{
	static const int maxFonts = 6;
	int nFonts = 0;
	fontInfo_t fonts[maxFonts];
	/// @}

	/// @name Frame
	/// @{
	int time = 0;
	float floatTime = 0;

	/// The current frame, as returned from bgfx::frame.
	uint32_t frameNo = 0;

	bool debugTextThisFrame = false;
	uint16_t debugTextY = 0;

	/// @remarks Resets to 0 every frame.
	bgfx::ViewId firstFreeViewId = 0;

	/// @}

	/// @name Framebuffers
	/// @{
	static const FrameBuffer defaultFb;
	FrameBuffer depthFb;
	FrameBuffer reflectionFb;
	FrameBuffer sceneFb;
	FrameBuffer sceneTempFb;
	uint8_t sceneBloomAttachment;
	uint8_t sceneDepthAttachment;
	static const size_t nBloomFrameBuffers = 2;
	FrameBuffer bloomFb[nBloomFrameBuffers];
	/// @}

	/// @name Noise
	/// @{
	static const int noiseSize = 256;
	float noiseTable[noiseSize];
	int noisePerm[noiseSize];
	/// @}

	/// @name Resource caches
	/// @{
	std::unique_ptr<TextureCache> textureCache;
	std::unique_ptr<MaterialCache> materialCache;
	std::unique_ptr<ModelCache> modelCache;
	/// @}

	/// @name Scene
	/// @{
	
	Transform mainCameraTransform;

	std::vector<vec3> sceneDebugAxis;
	std::vector<Bounds> sceneDebugBounds;
	std::vector<Entity> sceneEntities;

	struct Polygon
	{
		Material *material;
		int fogIndex;
		uint32_t firstVertex, nVertices;
	};

	std::vector<Polygon> scenePolygons;
	std::vector<Polygon *> sortedScenePolygons;
	std::vector<polyVert_t> scenePolygonVertices;
	mat3 sceneRotation;
	/// @}

	/// @name Shaders
	/// @{
	std::array<Shader, FragmentShaderId::Num> fragmentShaders;
	std::array<Shader, VertexShaderId::Num> vertexShaders;
	std::array<ShaderProgram, (int)ShaderProgramId::Num> shaderPrograms;
	/// @}

	/// @name Shadows
	/// @{
	FrameBuffer shadowMapFb;
	static const int shadowMapSize = 4096;
	/// @}

	/// @name Skybox portals
	/// @{
	bool skyboxPortalEnabled = false;
	SceneDefinition skyboxPortalScene;
	/// @}

	/// @name SMAA
	/// @{
	FrameBuffer smaaBlendFb, smaaEdgesFb;
	bgfx::TextureHandle smaaAreaTex = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle smaaSearchTex = BGFX_INVALID_HANDLE;
	/// @}

	/// @name Stretchpic
	/// @{
	vec4 stretchPicColor;
	Material *stretchPicMaterial = nullptr;
	bgfx::ViewId stretchPicViewId = UINT8_MAX;
	std::vector<Vertex> stretchPicVertices;
	std::vector<uint16_t> stretchPicIndices;
	/// @}

	/// @name Uniforms
	/// @{
	std::unique_ptr<Uniforms> uniforms;
	std::unique_ptr<Uniforms_Entity> entityUniforms;
	std::unique_ptr<Uniforms_Material> matUniforms;
	std::unique_ptr<Uniforms_MaterialStage> matStageUniforms;
	/// @}

	/// @name Derived from console variables
	/// @{
	AntiAliasing aa;
	bool bloomEnabled;
	bool extraDynamicLightsEnabled;
	bool fastPathEnabled;
	bool lerpTextureAnimationEnabled;
	bool maxAnisotropyEnabled;
	bool softSpritesEnabled;
	bool sunLightEnabled;
	bool waterReflectionsEnabled;
	/// @}
	
	bool captureFrame = false;
	const Entity *currentEntity = nullptr;
	DebugDraw debugDraw = DebugDraw::None;
	std::unique_ptr<DynamicLightManager> dlightManager;
	float halfTexelOffset = 0;
	bool isTextureOriginBottomLeft = false;
	vec2 lastCameraDepthRange; // for debug drawing
	SunLight sunLight;

	/// Convert from our coordinate system (looking down X) to OpenGL's coordinate system (looking down -Z)
	static const mat4 toOpenGlMatrix;
};

extern std::unique_ptr<Main> s_main;

DebugDraw DebugDrawFromString(const char *s);
bool IsMsaa(AntiAliasing aa);
bgfx::ViewId PushView(const FrameBuffer &frameBuffer, uint16_t clearFlags, const mat4 &viewMatrix, const mat4 &projectionMatrix, Rect rect, int flags = 0);
void RenderScreenSpaceQuad(const char *viewName, const FrameBuffer &frameBuffer, ShaderProgramId::Enum program, uint64_t state, uint16_t clearFlags = BGFX_CLEAR_NONE, bool originBottomLeft = false, Rect rect = Rect());
void SetWindowGamma();

} // namespace main
} // namespace renderer
