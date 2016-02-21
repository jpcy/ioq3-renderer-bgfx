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

enum class DebugDraw
{
	None,
	Depth,
	DynamicLight,
	Luminance
};

/*
Cells texture:
uint16_t offset into indices texture

Indices texture:
uint8_t num lights
uint8_t light index (0...n) into lights texture

Lights texture:
DynamicLight struct (0...n)
*/
class DynamicLightManager
{
public:
	DynamicLightManager();
	~DynamicLightManager();
	void add(int frameNo, const DynamicLight &light);
	void clear();
	void contribute(int frameNo, vec3 position, vec3 *color, vec3 *direction) const;
	bgfx::TextureHandle getCellsTexture() const { return cellsTexture_; }
	bgfx::TextureHandle getIndicesTexture() const { return indicesTexture_; }
	bgfx::TextureHandle getLightsTexture() const { return lightsTexture_; }
	void initializeGrid();
	void updateTextures(int frameNo);
	void updateUniforms(Uniforms *uniforms);

	static const size_t maxLights = 256;

private:
	void decodeAssignedLight(uint32_t value, vec3b *cellPosition, uint8_t *lightIndex) const;
	uint32_t encodeAssignedLight(vec3b cellPosition, uint8_t lightIndex) const;

	size_t cellIndexFromCellPosition(vec3b position) const;

	/// @remarks Result is clamped.
	vec3b cellPositionFromWorldspacePosition(vec3 position) const;

	bgfx::TextureHandle cellsTexture_;
	std::vector<uint16_t> cellsTextureData_[BGFX_NUM_BUFFER_FRAMES];
	uint16_t cellsTextureSize_;

	bgfx::TextureHandle indicesTexture_;
	std::vector<uint8_t> indicesTextureData_[BGFX_NUM_BUFFER_FRAMES];
	uint16_t indicesTextureSize_;

	std::vector<uint32_t> assignedLights_;
	vec3i cellSize_;
	vec3 gridOffset_;
	vec3b gridSize_;
	DynamicLight lights_[BGFX_NUM_BUFFER_FRAMES][maxLights];
	uint8_t nLights_;
	bgfx::TextureHandle lightsTexture_;
	uint16_t lightsTextureSize_;
};

class Main
{
public:
	Main();
	~Main();
	void initialize();
	void registerFont(const char *fontName, int pointSize, fontInfo_t *font);

	const Entity *getCurrentEntity() const { return currentEntity_; }
	float getFloatTime() const { return floatTime_; }
	int getFrameNo() const { return frameNo_; }
	float getNoise(float x, float y, float z, float t) const;
	int getTime() const { return time_; }
	bool isMirrorCamera() const { return isMirrorCamera_; }

	void setColor(vec4 c) { stretchPicColor_ = c; }
	void setSunLight(const SunLight &sunLight) { sunLight_ = sunLight; }
	void debugPrint(const char *format, ...);
	void drawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex);
	void drawStretchRaw(int x, int y, int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty);
	void uploadCinematic(int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty);
	void loadWorld(const char *name);
	void addDynamicLightToScene(const DynamicLight &light);
	void addEntityToScene(const refEntity_t *entity);
	void addPolyToScene(qhandle_t hShader, int nVerts, const polyVert_t *verts, int nPolys);
	void renderScene(const refdef_t *def);
	void endFrame();
	bool sampleLight(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir);

	/// @brief Called by the material cache when a material is created.
	void onMaterialCreate(Material *material);

	/// @brief Called by the model cache when a model is created.
	void onModelCreate(Model *model);

private:
	struct GenericShaderProgramVariant
	{
		enum
		{
			None       = 0,
			AlphaTest  = 1<<0,
			SoftSprite = 1<<1,
			Mask       = (1<<2) - 1
		};
	};

	struct ShaderProgramId
	{
		enum Enum
		{
			AdaptedLuminance,
			Depth,
			Depth_AlphaTest,
			Fog,
			FXAA,
			Generic,
			LinearDepth = Generic + GenericShaderProgramVariant::Mask + 1,
			Luminance,
			LuminanceDownsample,
			Texture,
			TextureColor,
			TextureSingleChannel,
			ToneMap,
			Num
		};
	};

	struct Rect
	{
		Rect() : x(0), y(0), w(0), h(0) {}
		Rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
		int x, y, w, h;
	};

	struct PushViewFlags
	{
		enum
		{
			Sequential = 1<<0
		};
	};

	void debugDraw(const FrameBuffer &texture, int x = 0, int y = 0, bool singleChannel = true);
	void debugDraw(bgfx::TextureHandle texture, int x = 0, int y = 0, bool singleChannel = true);
	uint8_t pushView(const FrameBuffer &frameBuffer, uint16_t clearFlags, const mat4 &viewMatrix, const mat4 &projectionMatrix, Rect rect, int flags = 0);
	void flushStretchPics();
	void renderCamera(uint8_t visCacheId, vec3 pvsPosition, vec3 position, mat3 rotation, Rect rect, vec2 fov, const uint8_t *areaMask);
	void renderScreenSpaceQuad(const FrameBuffer &frameBuffer, ShaderProgramId::Enum program, uint64_t state, bool originBottomLeft = false, Rect rect = Rect());
	void setTexelOffsetsDownsample2x2(int width, int height);
	void setTexelOffsetsDownsample4x4(int width, int height);

	/// @name Entity rendering
	/// @{
	void renderEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity);
	void renderLightningEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity);
	void renderRailCoreEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity);
	void renderRailCore(DrawCallList *drawCallList, vec3 start, vec3 end, vec3 up, float length, float spanWidth, Material *mat, vec4 color, Entity *entity);
	void renderRailRingsEntity(DrawCallList *drawCallList, Entity *entity);
	void renderSpriteEntity(DrawCallList *drawCallList, mat3 viewRotation, Entity *entity);
	void setupEntityLighting(Entity *entity);
	/// @}

	float calculateExplosionLight(float entityShaderTime, float durationMilliseconds) const;

	/// @name Camera
	/// @{
	Frustum cameraFrustum_;
	DrawCallList drawCalls_;
	bool isMirrorCamera_ = false;

	/// Is the current camera in the world (not RDF_NOWORLDMODEL).
	bool isWorldCamera_;

	vec4 portalPlane_;
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
	static const FrameBuffer defaultFb_;
	FrameBuffer fxaaFb_;
	bgfx::TextureHandle fxaaColor_;
	FrameBuffer linearDepthFb_;
	FrameBuffer sceneFb_;
	bgfx::TextureHandle sceneFbColor_;
	bgfx::TextureHandle sceneFbDepth_;
	/// @}

	/// @name Game-specific hacks
	/// @{
	Material *bfgExplosionMaterial_ = nullptr;
	Model *bfgMissibleModel_ = nullptr;
	Material *plasmaBallMaterial_ = nullptr;
	Material *plasmaExplosionMaterial_ = nullptr;
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
	std::unique_ptr<TextureCache> textureCache_;
	/// @}

	/// @name Scene
	/// @{
	std::vector<Entity> sceneEntities_;

	struct Polygon
	{
		Material *material;
		int fogIndex;
		size_t firstVertex, nVertices;
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

	enum class AntiAliasing
	{
		None,
		FXAA
	};

	AntiAliasing aa_;
	const Entity *currentEntity_ = nullptr;
	DebugDraw debugDraw_ = DebugDraw::None;
	std::unique_ptr<DynamicLightManager> dlightManager_;
	float halfTexelOffset_ = 0;
	bool isTextureOriginBottomLeft_ = false;
	uint8_t mainVisCacheId_, portalVisCacheId_;
	SunLight sunLight_;

	/// Convert from our coordinate system (looking down X) to OpenGL's coordinate system (looking down -Z)
	static const mat4 toOpenGlMatrix_;
};

DebugDraw DebugDrawFromString(const char *s);

} // namespace renderer
