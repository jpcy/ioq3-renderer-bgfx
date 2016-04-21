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

enum class AntiAliasing
{
	None,
	FXAA,
	MSAA2x,
	MSAA4x,
	MSAA8x,
	MSAA16x,
	SMAA
};

enum class DebugDraw
{
	None,
	Depth,
	DynamicLight,
	Luminance,
	Reflection,
	SMAA
};

class Main
{
public:
	Main();
	~Main();
	void initialize();

	const Entity *getCurrentEntity() const { return currentEntity_; }
	float getFloatTime() const { return floatTime_; }
	int getFrameNo() const { return frameNo_; }
	float getNoise(float x, float y, float z, float t) const;
	int getTime() const { return time_; }
	bool isCameraMirrored() const { return isCameraMirrored_; }

	void registerFont(const char *fontName, int pointSize, fontInfo_t *font);
	void setColor(vec4 c) { stretchPicColor_ = c; }
	void setSunLight(const SunLight &sunLight) { sunLight_ = sunLight; }
	void debugPrint(const char *text);
	void drawBounds(const Bounds &bounds);
	void drawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex);
	void drawStretchPicGradient(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex, vec4 gradientColor);
	void drawStretchRaw(int x, int y, int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty);
	void uploadCinematic(int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty);
	void loadWorld(const char *name);
	void addDynamicLightToScene(const DynamicLight &light);
	void addEntityToScene(const Entity &entity);
	void addPolyToScene(qhandle_t hShader, int nVerts, const polyVert_t *verts, int nPolys);
	void renderScene(const SceneDefinition &scene);
	void endFrame();
	bool sampleLight(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir);

private:
	struct DepthShaderProgramVariant
	{
		enum
		{
			None       = 0,
			AlphaTest  = 1 << 0,
			DepthRange = 1 << 1,
			Num        = 1 << 2
		};
	};

	struct FogShaderProgramVariant
	{
		enum
		{
			None       = 0,
			DepthRange = 1 << 0,
			Num        = 1 << 1
		};
	};

	/// @remarks Sync with generated GenericFragmentShaderVariant and GenericVertexShaderVariant. Order matters - fragment first.
	struct GenericShaderProgramVariant
	{
		enum
		{
			None          = 0,

			// Fragment
			AlphaTest     = 1 << 0,
			DynamicLights = 1 << 1,
			SoftSprite    = 1 << 2,

			// Vertex
			DepthRange    = 1 << 3,

			Num           = 1 << 4
		};
	};

	struct ShaderProgramId
	{
		enum Enum
		{
			AdaptedLuminance,
			Color,
			Depth,
			Fog = Depth + DepthShaderProgramVariant::Num,
			FXAA = Fog + FogShaderProgramVariant::Num,
			Generic,
			LinearDepth = Generic + GenericShaderProgramVariant::Num,
			Luminance,
			LuminanceDownsample,
			SMAABlendingWeightCalculation,
			SMAAEdgeDetection,
			SMAANeighborhoodBlending,
			Texture,
			TextureColor,
			TextureSingleChannel,
			ToneMap,
			Num
		};
	};

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
			UseClippingPlane     = 1<<2,
			UseStencilTest       = 1<<3
		};
	};

	void debugDraw(const FrameBuffer &texture, int x = 0, int y = 0, ShaderProgramId::Enum program = ShaderProgramId::Texture);
	void debugDraw(bgfx::TextureHandle texture, int x = 0, int y = 0, ShaderProgramId::Enum program = ShaderProgramId::Texture);
	uint8_t pushView(const FrameBuffer &frameBuffer, uint16_t clearFlags, const mat4 &viewMatrix, const mat4 &projectionMatrix, Rect rect, int flags = 0);
	void flushStretchPics();
	void renderCamera(uint8_t visCacheId, vec3 pvsPosition, vec3 position, mat3 rotation, Rect rect, vec2 fov, const uint8_t *areaMask, Plane clippingPlane = Plane(), int flags = 0);
	void renderPolygons();
	void renderScreenSpaceQuad(const FrameBuffer &frameBuffer, ShaderProgramId::Enum program, uint64_t state, uint16_t clearFlags = BGFX_CLEAR_NONE, bool originBottomLeft = false, Rect rect = Rect());
	void renderToStencil(const uint8_t viewId);
	void setTexelOffsetsDownsample2x2(int width, int height);
	void setTexelOffsetsDownsample4x4(int width, int height);

	/// @name Entity rendering
	/// @{
	void renderEntity(vec3 viewPosition, mat3 viewRotation, Frustum cameraFrustum, Entity *entity);
	void renderLightningEntity(vec3 viewPosition, mat3 viewRotation, Entity *entity);
	void renderRailCoreEntity(vec3 viewPosition, mat3 viewRotation, Entity *entity);
	void renderRailCore(vec3 start, vec3 end, vec3 up, float length, float spanWidth, Material *mat, vec4 color, Entity *entity);
	void renderRailRingsEntity(Entity *entity);
	void renderSpriteEntity(mat3 viewRotation, Entity *entity);
	void setupEntityLighting(Entity *entity);
	/// @}

	/// @name Camera
	/// @{
	DrawCallList drawCalls_;

	/// Flip face culling if true.
	bool isCameraMirrored_ = false;

	/// @}

	/// @name Fonts
	/// @{
	static const int maxFonts_ = 6;
	int nFonts_ = 0;
	fontInfo_t fonts_[maxFonts_];
	/// @}

	/// @name Frame
	/// @{
	int time_ = 0;
	float floatTime_ = 0;

	/// Incremented everytime endFrame() is called
	int frameNo_ = 0;

	uint16_t debugTextY = 0;

	/// @remarks Resets to 0 every frame.
	uint8_t firstFreeViewId_ = 0;

	/// @}

	/// @name Framebuffers
	/// @{
	struct SceneFrameBufferAttachment
	{
		enum
		{
			Color,
			Depth,
			Num
		};
	};

	static const FrameBuffer defaultFb_;
	FrameBuffer linearDepthFb_;
	FrameBuffer reflectionFb_;
	FrameBuffer sceneFb_;
	FrameBuffer sceneTempFb_;
	/// @}

	/// @name HDR luminance
	/// @{
	static const size_t nLuminanceFrameBuffers_ = 5;
	FrameBuffer luminanceFrameBuffers_[nLuminanceFrameBuffers_];
	const int luminanceFrameBufferSizes_[nLuminanceFrameBuffers_] = { 128, 64, 16, 4, 1 };
	FrameBuffer adaptedLuminanceFB_[2];
	int currentAdaptedLuminanceFB_ = 0;
	float lastAdaptedLuminanceTime_ = 0;
	/// @}

	/// @name Noise
	/// @{
	static const int noiseSize_ = 256;
	float noiseTable_[noiseSize_];
	int noisePerm_[noiseSize_];
	/// @}

	/// @name Resource caches
	/// @{
	std::unique_ptr<MaterialCache> materialCache_;
	std::unique_ptr<ModelCache> modelCache_;
	/// @}

	/// @name Scene
	/// @{
	
	/// Does the current scene contain the world (not RDF_NOWORLDMODEL).
	bool isWorldScene_;

	std::vector<vec3> sceneDebugAxis_;
	std::vector<Bounds> sceneDebugBounds_;
	std::vector<Entity> sceneEntities_;

	struct Polygon
	{
		Material *material;
		int fogIndex;
		uint32_t firstVertex, nVertices;
	};

	std::vector<Polygon> scenePolygons_;
	std::vector<Polygon *> sortedScenePolygons_;
	std::vector<polyVert_t> scenePolygonVertices_;
	mat3 sceneRotation_;
	/// @}

	/// @name Shaders
	/// @{
	std::array<Shader, FragmentShaderId::Num> fragmentShaders_;
	std::array<Shader, VertexShaderId::Num> vertexShaders_;
	std::array<ShaderProgram, (int)ShaderProgramId::Num> shaderPrograms_;
	/// @}

	/// @name Skybox portals
	/// @{
	bool skyboxPortalEnabled_ = false;
	SceneDefinition skyboxPortalScene_;
	uint8_t skyboxPortalVisCacheId;
	/// @}

	/// @name SMAA
	/// @{
	FrameBuffer smaaBlendFb_, smaaEdgesFb_;
	bgfx::TextureHandle smaaAreaTex_ = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle smaaSearchTex_ = BGFX_INVALID_HANDLE;
	/// @}

	/// @name Stretchpic
	/// @{
	vec4 stretchPicColor_;
	Material *stretchPicMaterial_ = nullptr;
	uint8_t stretchPicViewId_ = UINT8_MAX;
	std::vector<Vertex> stretchPicVertices_;
	std::vector<uint16_t> stretchPicIndices_;
	/// @}

	/// @name Uniforms
	/// @{
	std::unique_ptr<Uniforms> uniforms_;
	std::unique_ptr<Uniforms_Entity> entityUniforms_;
	std::unique_ptr<Uniforms_Material> matUniforms_;
	std::unique_ptr<Uniforms_MaterialStage> matStageUniforms_;
	/// @}

	AntiAliasing aa_, aaHud_;
	const Entity *currentEntity_ = nullptr;
	DebugDraw debugDraw_ = DebugDraw::None;
	std::unique_ptr<DynamicLightManager> dlightManager_;
	float halfTexelOffset_ = 0;
	bool isTextureOriginBottomLeft_ = false;
	uint8_t mainVisCacheId_, portalVisCacheId_, reflectionVisCacheId_;
	bool softSpritesEnabled_ = false;
	SunLight sunLight_;

	/// Convert from our coordinate system (looking down X) to OpenGL's coordinate system (looking down -Z)
	static const mat4 toOpenGlMatrix_;
};

DebugDraw DebugDrawFromString(const char *s);

} // namespace renderer
