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
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <vector>
#include "float.h"

#if !defined(_MSC_VER) && __cplusplus < 201402L
namespace std {
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}
#endif

extern "C"
{
#ifdef USE_LOCAL_HEADERS
#include "SDL.h"
#include "SDL_syswm.h"
#else
#include <SDL.h>
#include <SDL_syswm.h>
#endif
#undef main
}

#undef min
#undef max
#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "bx/debug.h"
#include "bx/math.h"
#include "bx/string.h"
#include "bx/timer.h"

#define BGFX_NUM_BUFFER_FRAMES 3

#include "../math/half.h"
#include "../math/Math.h"
using namespace math;
#include "Interface.h"
#include "../../shaders/SharedDefines.sh"

#undef LoadImage
#undef major
#undef minor
#undef None

namespace renderer {

struct Entity;
class Material;
class Model;
class ReadOnlyFile;
struct SceneDefinition;
class Skin;
struct SunLight;
class Texture;
struct Uniforms;
struct Uniforms_Material;
struct Uniforms_MaterialStage;
struct Vertex;

struct ConsoleVariables
{
	void initialize();

	ConsoleVariable backend;
	ConsoleVariable bgfx_stats;
	ConsoleVariable bloomScale;
	ConsoleVariable debug;
	ConsoleVariable debugDraw;
	ConsoleVariable debugDrawSize;
	ConsoleVariable dynamicLightIntensity;
	ConsoleVariable dynamicLightScale;
	ConsoleVariable picmip;
	ConsoleVariable railWidth;
	ConsoleVariable railCoreWidth;
	ConsoleVariable railSegmentLength;
	ConsoleVariable screenshotJpegQuality;
	ConsoleVariable shadowDepthBias;
	ConsoleVariable shadowNormalBias;
	ConsoleVariable shadowSlopeScaleDepthBias;
	ConsoleVariable sunLightIntensity;
	ConsoleVariable textureVariation;
	ConsoleVariable wireframe;

	/// @name Gamma
	/// @{
	ConsoleVariable gamma;
	ConsoleVariable ignoreHardwareGamma;
	/// @}

	/// @name Window
	/// @{
	ConsoleVariable allowResize;
	ConsoleVariable centerWindow;
	ConsoleVariable customheight;
	ConsoleVariable customwidth;
	ConsoleVariable customPixelAspect;
	ConsoleVariable fullscreen;
	ConsoleVariable mode;
	ConsoleVariable noborder;
	/// @}
};

struct DrawCallFlags
{
	enum
	{
		None   = 0,

		/// @brief Either world surfaceFlags SURF_SKY (e.g. space maps with no material skyparms) or Material::isSky (everything else)
		Sky    = 1<<0,

		Skybox = 1<<1
	};
};

struct DrawCall
{
	bool operator<(const DrawCall &other) const;

	enum class BufferType
	{
		Static,
		Dynamic,
		Transient,
	};

	struct VertexBuffer
	{
		BufferType type;
		bgfx::VertexBufferHandle staticHandle;
		bgfx::DynamicVertexBufferHandle dynamicHandle;
		bgfx::TransientVertexBuffer transientHandle;
		uint32_t firstVertex = 0;
		uint32_t nVertices = 0;
	};

	struct IndexBuffer
	{
		BufferType type;
		bgfx::IndexBufferHandle staticHandle;
		bgfx::DynamicIndexBufferHandle dynamicHandle;
		bgfx::TransientIndexBuffer transientHandle;
		uint32_t firstIndex = 0;
		uint32_t nIndices = 0;
	};

	bool dynamicLighting = true;
	const Entity *entity = nullptr;
	int flags = DrawCallFlags::None;
	int fogIndex = -1;
	IndexBuffer ib;
	Material *material = nullptr;
	mat4 modelMatrix = mat4::identity;
	int skyboxSide;
	float softSpriteDepth = 0;
	uint8_t sort = 0;
	uint64_t state = BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE;
	VertexBuffer vb;
	float zOffset = 0.0f;
	float zScale = 0.0f;
};

typedef std::vector<DrawCall> DrawCallList;

struct DynamicIndexBuffer
{
	DynamicIndexBuffer() { handle.idx = bgfx::kInvalidHandle; }
	~DynamicIndexBuffer() { if (bgfx::isValid(handle)) bgfx::destroy(handle); }
	bgfx::DynamicIndexBufferHandle handle;
};

struct DynamicLight
{
	enum
	{
		Capsule = DLIGHT_CAPSULE,
		Point   = DLIGHT_POINT
	};

	/// @remarks Only xyz used.
	vec4 capsuleEnd;

	/// @remarks rgb is color (linear space), alpha is radius/intensity.
	vec4 color_radius;

	/// @remarks w is type.
	vec4 position_type;
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
	void add(uint32_t frameNo, const DynamicLight &light);
	void clear();
	void contribute(uint32_t frameNo, vec3 position, vec3 *color, vec3 *direction) const;
	bgfx::TextureHandle getCellsTexture() const { return cellsTexture_; }
	bgfx::TextureHandle getIndicesTexture() const { return indicesTexture_; }
	bgfx::TextureHandle getLightsTexture() const { return lightsTexture_; }
	void initializeGrid();
	void updateTextures(uint32_t frameNo);
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

struct EntityFlags
{
	enum
	{
		DepthHack        = 1<<0,
		FirstPerson      = 1<<1,
		LightingPosition = 1<<2,
		ThirdPerson      = 1<<3
	};
};

enum class EntityType
{
	Beam,
	Lightning, 
	Model,
	Poly,
	Portal,
	RailCore,
	RailRings,
	Sprite
};

struct Entity
{
	/// @name Interface
	/// @{

	float angle;
	qhandle_t customMaterial;
	qhandle_t customSkin;
	int flags;
	int frame, torsoFrame;
	int oldFrame, oldTorsoFrame;
	qhandle_t handle;
	float lerp, torsoLerp;
	vec3 lightingPosition;

	/// Color used by MaterialColorGen::Entity and MaterialColorGen::OneMinusEntity.
	vec4 materialColor;

	/// Texture coordinate used by MaterialTexMod::EntityTranslate.
	vec2 materialTexCoord;

	float materialTime;

	/// Axis are not normalized, i.e. they have scale.
	bool nonNormalizedAxes;

	vec3 position, oldPosition;
	float radius;
	mat3 rotation, torsoRotation;
	int skinNum;
	EntityType type;

	/// @}

	/// @name Local
	/// @{

	/// @remarks Used for environment mapping, alphagen specular and alphagen portal.
	vec3 localViewPosition;

	/// Normalized direction towards light, in world space.
	vec3 lightDir;

	/// Color normalized to 0-255.
	vec3 ambientLight;

	vec3 directedLight;

	/// @}
};

struct FrameBuffer
{
	FrameBuffer() { handle.idx = bgfx::kInvalidHandle; }
	~FrameBuffer() { if (bgfx::isValid(handle)) bgfx::destroy(handle); }
	bgfx::FrameBufferHandle handle;
};

struct ImageFlags
{
	enum
	{
		DataIsBgfxMemory = 1<<0,
	};
};

struct Image
{
	int width = 0;
	int height = 0;
	int nComponents = 0;
	int nMips = 1;
	uint8_t *data = nullptr;
	uint32_t dataSize = 0;
	bgfx::ReleaseFn release = nullptr;
	int flags = 0;
};

struct CreateImageFlags
{
	enum
	{
		GenerateMipmaps    = 1<<0,
		Picmip             = 1<<1
	};
};

Image CreateImage(int width, int height, int nComponents, uint8_t *data, int flags = 0);
Image LoadImage(const char *filename, int flags = 0);

struct IndexBuffer
{
	IndexBuffer() { handle.idx = bgfx::kInvalidHandle; }
	~IndexBuffer() { if (bgfx::isValid(handle)) bgfx::destroy(handle); }
	bgfx::IndexBufferHandle handle;
};

#if defined(USE_LIGHT_BAKER)
namespace light_baker
{
	void Start(int nSamples);
	void Stop();
	void Update(uint32_t frameNo);
	void Shutdown();
}
#endif

namespace main
{
	void AddDynamicLightToScene(const DynamicLight &light);
	void AddEntityToScene(const Entity &entity);
	void AddPolyToScene(qhandle_t hShader, int nVerts, const polyVert_t *verts, int nPolys);
	bool AreWaterReflectionsEnabled();
	bool AreExtraDynamicLightsEnabled();
	float CalculateNoise(float x, float y, float z, float t);
	void DebugPrint(const char *format, ...);
	void DrawAxis(vec3 position);
	void DrawBounds(const Bounds &bounds);
	void DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex);
	void DrawStretchPicGradient(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex, vec4 gradientColor);
	void DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty);
	void EndFrame();
	const Entity *GetCurrentEntity();
	float GetFloatTime();
	Transform GetMainCameraTransform();
	void Initialize();
	bool IsCameraMirrored();
	bool IsLerpTextureAnimationEnabled();
	bool IsMaxAnisotropyEnabled();
	void LoadWorld(const char *name); 
	void RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
	void RenderHemicube(const FrameBuffer &frameBuffer, vec3 position, const vec3 forward, const vec3 up, vec2i rectOffset, int faceSize, bool skipUnlitSurfaces);
	void RenderScene(const SceneDefinition &scene);
	bool SampleLight(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir);
	void SetColor(vec4 c);
	const SunLight &GetSunLight();
	void SetSunLight(const SunLight &sunLight); 
	void Shutdown(bool destroyWindow);
	void UploadCinematic(int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty);
}

enum class MaterialAdjustColorsForFog
{
	None,
	ModulateRGB,
	ModulateRGBA,
	ModulateAlpha
};

enum class MaterialAlphaGen
{
	Identity         = AGEN_IDENTITY,
	LightingSpecular = AGEN_LIGHTING_SPECULAR,
	Portal           = AGEN_PORTAL,
	Water            = AGEN_WATER,
	Skip,
	Entity,
	OneMinusEntity,
	NormalZFade,
	Vertex,
	OneMinusVertex,
	Waveform,
	Const
};

enum class MaterialAlphaTest
{
	None,
	GT_0   = ATEST_GT_0,
	LT_128 = ATEST_LT_128,
	GE_128 = ATEST_GE_128
};

enum class MaterialColorGen
{
	Bad,
	Identity        = CGEN_IDENTITY, // always (1,1,1,1)
	LightingDiffuse,
	IdentityLighting,	// Main::identityLight
	Entity,			// grabbed from entity's modulate field
	OneMinusEntity,	// grabbed from 1 - entity.modulate
	ExactVertex     = CGEN_EXACT_VERTEX, // tess.vertexColors
	Vertex          = CGEN_VERTEX, // tess.vertexColors * Main::identityLight
	ExactVertexLit,	// like ExactVertex but takes a light direction from the lightgrid
	VertexLit,		// like Vertex but takes a light direction from the lightgrid
	OneMinusVertex,
	Waveform,			// programmatically generated
	Fog,				// standard fog
	Const				// fixed color
};

enum class MaterialCullType
{
	FrontSided,
	BackSided,
	TwoSided
};

enum class MaterialDeform
{
	None  = DGEN_NONE,
	Bulge = DGEN_BULGE,
	Move  = DGEN_MOVE,
	Wave  = DGEN_WAVE,
	Autosprite,
	Autosprite2,
	Normals,
	ProjectionShadow,
	Text0,
	Text1,
	Text2,
	Text3,
	Text4,
	Text5,
	Text6,
	Text7
};

enum class MaterialWaveformGenFunc
{
	None            = DGEN_WAVE_NONE,
	Sin             = DGEN_WAVE_SIN,
	Square          = DGEN_WAVE_SQUARE,
	Triangle        = DGEN_WAVE_TRIANGLE,
	Sawtooth        = DGEN_WAVE_SAWTOOTH,
	InverseSawtooth = DGEN_WAVE_INVERSE_SAWTOOTH,
	Noise
};

struct MaterialWaveForm
{
	MaterialWaveformGenFunc func = MaterialWaveformGenFunc::None;
	float base = 0;
	float amplitude = 0;
	float phase = 0;
	float frequency = 0;
};

struct MaterialDeformStage
{
	/// Vertex coordinate modification type.
	MaterialDeform deformation = MaterialDeform::None;

	vec3 moveVector;
	MaterialWaveForm deformationWave;
	float deformationSpread = 0;

	float bulgeWidth = 0;
	float bulgeHeight = 0;
	float bulgeSpeed = 0;
};

struct MaterialFogParms
{
	vec3 color;
	float depthForOpaque = 0;
};

enum class MaterialFogPass
{
	/// Surface is translucent and will just be adjusted properly.
	None,

	/// Surface is opaque but possibly alpha tested.
	Equal,

	/// Surface is translucent, but still needs a fog pass (fog surface).
	LessOrEqual
};

enum class MaterialLight
{
	None   = LIGHT_NONE,
	Map    = LIGHT_MAP,
	Vertex = LIGHT_VERTEX,
	Vector = LIGHT_VECTOR
};

struct MaterialLightmapId
{
	enum
	{
		/// Shader is for 2D rendering.
		StretchPic = -4,

		/// Pre-lit triangle models.
		Vertex = -3,

		White = -2,
		None = -1
	};
};

enum class MaterialReflective
{
	None,
	FrontSide,
	BackSide
};

struct MaterialSkyParms
{
	MaterialSkyParms() : outerbox(), innerbox() {}

	float cloudHeight = 0;
	const Texture *outerbox[6];
	const Texture *innerbox[6];
};

struct MaterialSort
{
	enum
	{
		Bad,

		/// Mirrors, portals, viewscreens.
		Portal,

		/// Sky box.
		Environment,
	
		Opaque,

		/// Scorch marks, etc.
		Decal,

		/// Ladders, grates, grills that may have small blended edges in addition to alpha test.
		SeeThrough,

		Banner,
		Fog,

		/// For items that should be drawn in front of the water plane.
		Underwater,

		/// Regular transparency and filters. Generally only used for additive type effects.
		Blend0,

		Blend1,
		Blend2,
		Blend3,
		Blend6,
		StencilShadow,

		/// Gun smoke puffs.
		AlmostNearest,

		/// Blood blobs.
		Nearest
	};
};

enum class MaterialStageType
{
	ColorMap = 0,
	DiffuseMap = 0,
	NormalMap,
	NormalParallaxMap,
	SpecularMap,
	GLSL
};

enum class MaterialStageTextureAnimationLerp
{
	Disabled,
	Clamp,
	Wrap
};

enum class MaterialTexCoordGen
{
	None = TCGEN_NONE,
	EnvironmentMapped = TCGEN_ENVIRONMENT_MAPPED,
	Fog = TCGEN_FOG,
	Fragment = TCGEN_FRAGMENT,
	Lightmap = TCGEN_LIGHTMAP,
	Texture = TCGEN_TEXTURE,

	/// S and T from world coordinates.
	Vector = TCGEN_VECTOR,

	/// Clear to 0,0
	Identity
};

enum class MaterialTexMod
{
	None,
	Transform,
	Turbulent,
	Scroll,
	Scale,
	Stretch,
	Rotate,
	EntityTranslate
};

struct MaterialTexModInfo
{
	MaterialTexMod type = MaterialTexMod::None;

// used for MaterialTexMod::Turbulent and MaterialTexMod::Stretch
	MaterialWaveForm wave;

// used for MaterialTexMod::Transform
	float matrix[2][2];		// s' = s * m[0][0] + t * m[1][0] + trans[0]
	float translate[2];		// t' = s * m[0][1] + t * m[0][1] + trans[1]

// used for MaterialTexMod::Scale
	vec2 scale;			// s *= scale[0]
// t *= scale[1]

// used for MaterialTexMod::Scroll
	vec2 scroll;			// s' = s + scroll[0] * time
// t' = t + scroll[1] * time

// + = clockwise
// - = counterclockwise
	float rotateSpeed = 0;
};

struct MaterialTextureBundle
{
	MaterialTextureBundle() : textures() {}

	static const size_t maxTexMods = 4;

	static const size_t maxImageAnimations = 8;
	const Texture *textures[maxImageAnimations];
	int numImageAnimations = 0;
	float imageAnimationSpeed = 0;

	MaterialTexCoordGen tcGen = MaterialTexCoordGen::None;
	vec3 tcGenVectors[2];

	int numTexMods = 0;
	MaterialTexModInfo texMods[maxTexMods];

	int videoMapHandle = 0;
	bool isLightmap = false;
	bool isVideoMap = false;
};

/// Indices into MaterialStage::bundle
struct MaterialTextureBundleIndex
{
	enum
	{
		DiffuseMap,
		Lightmap,
		NormalMap,
		Deluxemap,
		Specularmap,
		NumMaterialTextureBundles
	};
};

struct MaterialStageSetUniformsFlags
{
	enum
	{
		ColorGen = 1<<0,
		TexGen   = 1<<1,
		All      = (1<<2) - 1
	};
};

struct MaterialStage
{
	bool active = false;
	Material *material = nullptr;

	MaterialTextureBundle bundles[MaterialTextureBundleIndex::NumMaterialTextureBundles];

	MaterialWaveForm rgbWave;
	MaterialColorGen rgbGen = MaterialColorGen::Bad;

	MaterialWaveForm alphaWave;
	MaterialAlphaGen alphaGen = MaterialAlphaGen::Identity;

	MaterialStageTextureAnimationLerp textureAnimationLerp = MaterialStageTextureAnimationLerp::Disabled;

	vec4 constantColor; // for MaterialColorGen::Const and MaterialAlphaGen::Const

	uint64_t depthTestBits = BGFX_STATE_DEPTH_TEST_LEQUAL;

	bool depthWrite = true;

	MaterialAlphaTest alphaTest = MaterialAlphaTest::None;

	uint64_t blendSrc = 0;
	uint64_t blendDst = 0;

	MaterialAdjustColorsForFog adjustColorsForFog = MaterialAdjustColorsForFog::None;

	bool isDetail = false;

	MaterialStageType type = MaterialStageType::ColorMap;
	MaterialLight light = MaterialLight::None;
	bool bloom = false;
	bool textureVariation = false;

	vec4 normalScale;
	vec4 specularScale;

	vec2 zFadeBounds; // for MaterialAlphaGen::NormalZFade

	vec4 getFogColorMask() const;
	uint64_t getState() const;
	void setShaderUniforms(Uniforms_MaterialStage *uniforms, int flags = MaterialStageSetUniformsFlags::All) const;
	void setTextureSamplers(Uniforms_MaterialStage *uniforms) const;

private:
	/// @name Calculate
	/// @{
	bool shouldLerpTextureAnimation() const;
	void calculateTextureAnimation(int *frame, int *nextFrame, float *fraction) const;

	float *tableForFunc(MaterialWaveformGenFunc func) const;
	float evaluateWaveForm(const MaterialWaveForm &wf) const;
	float evaluateWaveFormClamped(const MaterialWaveForm &wf) const;
	void calculateTexMods(vec4 *outMatrix, vec4 *outOffTurb) const;
	void calculateTurbulentFactors(const MaterialWaveForm &wf, float *amplitude, float *now) const;
	void calculateScaleTexMatrix(vec2 scale, float *matrix) const;
	void calculateScrollTexMatrix(vec2 scrollSpeed, float *matrix) const;
	void calculateStretchTexMatrix(const MaterialWaveForm &wf, float *matrix) const;
	void calculateTransformTexMatrix(const MaterialTexModInfo &tmi, float *matrix) const;
	void calculateRotateTexMatrix(float degsPerSecond, float *matrix) const;
	float calculateWaveColorSingle(const MaterialWaveForm &wf) const;
	float calculateWaveAlphaSingle(const MaterialWaveForm &wf) const;

	/// rgbGen and alphaGen
	void calculateColors(vec4 *baseColor, vec4 *vertColor) const;

	/// @}
};

class Material
{
	friend class MaterialCache;
	friend struct MaterialStage;

public:
	Material() {}
	Material(const char *name);
	size_t getNumStages() const { return numUnfoggedPasses; }
	int32_t getDepth() const { return (int32_t)sort; }

	char name[MAX_QPATH];		// game path, including extension
	int lightmapIndex = MaterialLightmapId::None;			// for a shader to match, both name and lightmapIndex must match

	int index = 0;					// this shader == tr.shaders[index]
	int sortedIndex = 0;			// this shader == tr.sortedShaders[sortedIndex]

	float sort = 0;					// lower numbered shaders draw before higher numbered

	bool defaultShader = false;			// we want to return index 0 if the shader failed to
// load for some reason, but R_FindShader should
// still keep a name allocated for it, so if
// something calls RE_RegisterShader again with
// the same name, we don't try looking for it again

	bool explicitlyDefined = false;		// found in a .shader file
	float surfaceLight = 0; // area light
	unsigned int surfaceFlags = 0;			// if explicitlyDefined, this will have SURF_* flags
	unsigned int contentFlags = 0;

	bool entityMergable = false;			// merge across entites optimizable (smoke, blood)

	bool isSky = false;
	MaterialSkyParms sky;
	MaterialFogParms fogParms;
	bool noFog = false;

	float portalRange = 256;			// distance to fog out at
	bool isPortal = false;

	MaterialReflective reflective = MaterialReflective::None;
	Material *reflectiveFrontSideMaterial = nullptr;

	MaterialCullType cullType = MaterialCullType::FrontSided;
	bool polygonOffset = false;			// set for decals and other items that must be offset
	bool noMipMaps = false;				// for console fonts, 2D elements, etc.
	bool noPicMip = false;				// for images that must always be full resolution

	MaterialFogPass fogPass = MaterialFogPass::None;				// draw a blended pass, possibly with depth test equals

	int vertexAttribs = 0;          // not all shaders will need all data to be gathered

	int numDeforms = 0;
	static const size_t maxDeforms = MAX_DEFORMS;
	MaterialDeformStage deforms[maxDeforms];

	int numUnfoggedPasses = 0;
	static const size_t maxStages = 8;
	MaterialStage stages[maxStages];

	float clampTime = 0;                                  // time this shader is clamped to
	float timeOffset = 0;                                 // current time offset for this shader

	Material *remappedShader = nullptr;                  // current shader this one is remapped too

	Material *next = nullptr;

private:

	/// @name Parsing
	/// @{

	/// @param text The text pointer at the explicit text definition of the
	bool parse(char **text);

	vec3 parseVector(char **text, bool *result = nullptr) const;
	bool parseStage(MaterialStage *stage, char **text);
	MaterialWaveForm parseWaveForm(char **text) const;
	MaterialTexModInfo parseTexMod(char *buffer) const;
	MaterialDeformStage parseDeform(char **text) const;
	void parseSkyParms(char **text);

	MaterialAlphaTest alphaTestFromName(const char *name) const;
	uint64_t srcBlendModeFromName(const char *name) const;
	uint64_t dstBlendModeFromName(const char *name) const;
	MaterialWaveformGenFunc genFuncFromName(const char *name) const;
	float sortFromName(const char *name) const;

	/// @}

	/// @name State
	/// @{

	void finish();
	int collapseStagesToGLSL();

	/// @}

public:
	/// @name Calculate
	/// @{

	/// @brief Set the time for this material.
	/// @return The adjusted time.
	/// @remarks Used for animated textures, waveforms etc.
	float setTime(float time);

	bool hasAutoSpriteDeform() const;
	void doAutoSpriteDeform(const mat3 &sceneRotation, Vertex *vertices, uint32_t nVertices, uint16_t *indices, uint32_t nIndices, float *softSpriteDepth) const;
	void setDeformUniforms(Uniforms_Material *uniforms) const;

private:
	float time_;

	/// @}
};

class MaterialCache
{
public:
	MaterialCache();
	Material *createMaterial(const Material &base);
	Material *findMaterial(const char *name, int lightmapIndex = MaterialLightmapId::StretchPic, bool mipRawImage = true);
	void remapMaterial(const char *oldName, const char *newName, const char *offsetTime);
	Material *getMaterial(int handle) { return materials_[handle].get(); }
	Material *getDefaultMaterial() { return defaultMaterial_; }
	void printMaterials() const;

	Skin *findSkin(const char *name);
	Skin *getSkin(qhandle_t handle);

private:
	size_t generateHash(const char *fname, size_t size);

	void createInternalShaders();

	/// Finds and loads all .shader files, combining them into a single large text block that can be scanned for shader names.
	void scanAndLoadShaderFiles();

	void createExternalShaders();

	/// Scans the combined text description of all the shader files for the given shader name.
	/// @return If found, a valid shader. NULL if not found.
	char *findShaderInShaderText(const char *name);

	static const size_t maxShaderFiles_ = 4096;
	std::vector<char> shaderText_;

	std::vector<std::unique_ptr<Material>> materials_;

	static const size_t hashTableSize_ = 1024;
	Material *hashTable_[hashTableSize_];

	static const size_t textHashTableSize_ = 2048;
	char **textHashTable_[textHashTableSize_];

	Material *defaultMaterial_;

	std::vector<std::unique_ptr<Skin>> skins_;
};

namespace meta
{
	void Initialize();
	void OnEntityAddedToScene(const Entity &entity, bool isWorldScene);

	/// @brief Called by the material cache when a material is created.
	void OnMaterialCreate(Material *material);

	/// @brief Called by the model cache when a model is created.
	void OnModelCreate(Model *model);
}

class Model
{
public:
	virtual ~Model() {}
	virtual bool load(const ReadOnlyFile &file) = 0;
	virtual Bounds getBounds() const = 0;
	virtual Material *getMaterial(size_t surfaceNo) const = 0;
	virtual bool isCulled(Entity *entity, const Frustum &cameraFrustum) const = 0;
	virtual void render(const mat3 &sceneRotation, DrawCallList *drawCallList, Entity *entity) = 0;

	/// @return Tag index. -1 if tag not found.
	virtual int lerpTag(const char *name, const Entity &entity, int startIndex, Transform *transform) const = 0;

	size_t getIndex() const { return index_; }
	const char *getName() const { return name_; }

	static std::unique_ptr<Model> createMD3(const char *name);
	static std::unique_ptr<Model> createMDC(const char *name);

#if defined(ENGINE_IORTCW)
	static std::unique_ptr<Model> createMDS(const char *name);
#endif

protected:
	char name_[MAX_QPATH];

private:
	size_t index_;
	Model *next_ = nullptr;

	friend class ModelCache;
};

class ModelCache
{
public:
	ModelCache();
	Model *findModel(const char *name);
	Model *addModel(std::unique_ptr<Model> model);
	Model *getModel(int handle) { return handle <= 0 ? nullptr : models_[handle - 1].get(); }

private:
	size_t generateHash(const char *fname, size_t size);

	std::vector<std::unique_ptr<Model>> models_;

	static const size_t hashTableSize_ = 1024;
	Model *hashTable_[hashTableSize_];
};

struct Patch
{
	// dynamic lighting information
	int				dlightBits;
	int             pshadowBits;

	// culling information
	Bounds cullBounds;
	vec3			cullOrigin;
	float			cullRadius;

	// indexes
	int             numIndexes;
	uint16_t      *indexes;

	// vertexes
	int             numVerts;
	Vertex      *verts;

	// BSP VBO offsets
	int             firstVert;
	int             firstIndex;
	uint16_t       minIndex;
	uint16_t       maxIndex;

	// SF_GRID specific variables after here

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3			lodOrigin;
	float			lodRadius;
	int				lodFixed;
	int				lodStitched;

	// vertexes
	int				width, height;
	float			*widthLodError;
	float			*heightLodError;
};

Patch *Patch_Subdivide(int width, int height, const Vertex *points);
void Patch_Free(Patch *grid);

#ifdef USE_PROFILER
namespace profiler
{
	void BeginFrame(uint32_t frameNo);
	void Print();
	void BeginEntry(const char *name);
	void EndEntry();

	struct ScopedEntry
	{
		ScopedEntry(const char *name) { BeginEntry(name); }
		~ScopedEntry() { EndEntry(); }
	};
}
#define PROFILE_SCOPED(x) profiler::ScopedEntry _profiler_x(#x);
#define PROFILE_BEGIN(x) profiler::BeginEntry(#x);
#define PROFILE_END profiler::EndEntry();
#else
#define PROFILER_INITIALIZE
#define PROFILE_SCOPED(x)
#define PROFILE_BEGIN(x)
#define PROFILE_END
#endif

class ReadOnlyFile
{
public:
	ReadOnlyFile(const char *filename) { length_ = interface::FS_ReadFile(filename, &data_); }
	~ReadOnlyFile() { if (data_) interface::FS_FreeReadFile(data_); }
	bool isValid() const { return data_ && length_ >= 0; }
	const uint8_t *getData() const { return data_; }
	uint8_t *getData() { return data_; }
	size_t getLength() const { return (size_t)length_; }

private:
	uint8_t *data_;
	long length_;
};

struct Rect
{
	Rect() : x(0), y(0), w(0), h(0) {}
	Rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
	int x, y, w, h;
};

struct SceneDefinitionFlags
{
	enum
	{
		Hyperspace           = 1<<0,
		SkyboxPortal         = 1<<1,
		ContainsSkyboxPortal = 1<<2,
		World                = 1<<3
	};
};

struct SceneDefinition
{
	uint8_t areaMask[MAX_MAP_AREA_BYTES];
	int flags;
	vec2 fov;
	vec3 position;
	Rect rect;
	mat3 rotation;
	int time;
};

struct Shader
{
	Shader() { handle.idx = bgfx::kInvalidHandle; }
	~Shader() { if (bgfx::isValid(handle)) bgfx::destroy(handle); }
	bgfx::ShaderHandle handle;
};

struct ShaderProgram
{
	ShaderProgram() { handle.idx = bgfx::kInvalidHandle; }
	~ShaderProgram() { if (bgfx::isValid(handle)) bgfx::destroy(handle); }
	bgfx::ProgramHandle handle;
};

class Skin
{
public:
	/// Create a skin by loading it from a file.
	Skin(const char *name, qhandle_t handle);

	/// Create a skin with a single surface using the supplied material.
	Skin(const char *name, qhandle_t handle, Material *material);

	bool hasSurfaces() const { return !surfaces_.empty(); }
	const char *getName() const { return name_; }
	qhandle_t getHandle() const { return handle_; }
	float getScale() const { return scale_; }

	Material *findMaterial(const char *surfaceName);
	const char *findModelName(const char *modelType) const;

private:
	static const size_t maxModels_ = 5;

	struct Model
	{
		char type[MAX_QPATH]; // md3_lower, md3_lbelt, md3_rbelt, etc.
		char name[MAX_QPATH]; // lower.md3, belt1.md3, etc.
	};

	struct Surface
	{
		char name[MAX_QPATH];
		Material *material;
	};

	/// Game path, including extension.
	char name_[MAX_QPATH];

	qhandle_t handle_;
	Model models_[maxModels_];
	size_t nModels_ = 0;
	std::vector<Surface> surfaces_;
	float scale_ = 0;
};

struct SkySurface
{
	Material *material;
	std::vector<Vertex> vertices;
};

/// @remarks Called when a sky material is parsed.
void Sky_InitializeTexCoords(float heightCloud);

void Sky_Render(DrawCallList *drawCallList, vec3 cameraPosition, float zMax, const SkySurface &surface);

struct StaticLightFlags
{
	enum
	{
		LinearAttenuation   = 1<<0,
		AngleAttenuation    = 1<<1,
		DistanceAttenuation = 1<<2,
		Spotlight           = 1<<3,
		DefaultMask         = AngleAttenuation | DistanceAttenuation
	};
};

struct StaticLight
{
	vec4 color;
	float envelope;
	int flags;
	float intensity;
	float photons;
	vec3 position;
	float radius;
	vec3 targetPosition;
};

struct SunLight
{
	SunLight() { direction.normalize(); }

	bool shadows = false;
	vec3 light = { 0, 0, 0 };
	vec3 direction = { 0.45f, 0.3f, 0.9f };
	float lightScale = 1;
	float shadowScale = 0.5f;
};

struct TextureFlags
{
	enum
	{
		None        = 0,
		Mipmap      = 1<<0,
		Mutable     = 1<<1,
		Picmip      = 1<<2,
		ClampToEdge = 1<<3,
	};
};

class Texture
{
public:
	void resize(int width, int height);
	void update(const bgfx::Memory *mem, int x, int y, int width, int height);
	int getFlags() const { return flags_; }
	bgfx::TextureHandle getHandle() const { return handle_; }
	const char *getName() const { return name_; }
	int getWidth() const { return width_; }
	int getHeight() const { return height_; }

private:
	void initialize(const char *name, const Image &image, int flags, bgfx::TextureFormat::Enum format);
	void initialize(const char *name, bgfx::TextureHandle handle);
	uint32_t calculateBgfxFlags() const;

	char name_[MAX_QPATH];
	int flags_;
	int width_, height_;
	int nMips_;
	bgfx::TextureFormat::Enum format_;
	bgfx::TextureHandle handle_;
	Texture *next_;

	friend class TextureCache;
};

class TextureCache
{
public:
	TextureCache();
	~TextureCache();
	Texture *create(const char *name, const Image &image, int flags = TextureFlags::None, bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8);
	Texture *create(const char *name, bgfx::TextureHandle handle);

	/// Finds or loads the given image.
	/// @return nullptr if it fails, not the default image.
	Texture *find(const char *name, int flags = TextureFlags::None);

	Texture *get(const char *name);
	const Texture *getDefault() const { return defaultTexture_; }
	const Texture *getIdentityLight() const { return identityLightTexture_; }
	const Texture *getNoise() const { return noiseTexture_; }
	const Texture *getWhite() const { return whiteTexture_; }
	Texture *getScratch(size_t index) { return scratchTextures_[index]; }
	void alias(Texture *from, Texture *to);

private:
	void hashTexture(Texture *texture);
	size_t generateHash(const char *name) const;

	static const size_t maxTextures_ = 2048;
	Texture textures_[maxTextures_];
	size_t nTextures_ = 0;
	static const size_t hashTableSize_ = 1024;
	Texture *hashTable_[hashTableSize_];
	static const int defaultImageSize_ = 16;
	static const uint32_t defaultImageDataSize_ = defaultImageSize_ * defaultImageSize_ * 4;
	uint8_t defaultImageData_[defaultImageDataSize_];
	uint8_t whiteImageData_[defaultImageDataSize_];
	uint8_t identityLightImageData_[defaultImageDataSize_];
	Texture *defaultTexture_, *identityLightTexture_, *whiteTexture_;
	static const int noiseImageSize_ = 256;
	static const uint32_t noiseImageDataSize_ = noiseImageSize_ * noiseImageSize_ * 4;
	uint8_t noiseImageData_[noiseImageDataSize_];
	Texture *noiseTexture_;
	static const size_t nScratchTextures_ = 32;
	uint8_t scratchImageData_[nScratchTextures_][defaultImageDataSize_];
	std::array<Texture *, nScratchTextures_> scratchTextures_;
	std::map<Texture *, Texture *> aliases_;
};

/// Texture units used by the generic shader(s).
struct TextureUnit
{
	enum
	{
		Diffuse             = TU_DIFFUSE,
		Diffuse2            = TU_DIFFUSE2,
		Light               = TU_LIGHT,
		Depth               = TU_DEPTH,
		DynamicLightCells   = TU_DYNAMIC_LIGHT_CELLS,
		DynamicLightIndices = TU_DYNAMIC_LIGHT_INDICES,
		DynamicLights       = TU_DYNAMIC_LIGHTS,
		ShadowMap           = TU_SHADOWMAP,
		Noise               = TU_NOISE
	};
};

struct Uniform_int
{
	Uniform_int(const char *name, uint16_t num = 1) { handle = bgfx::createUniform(name, bgfx::UniformType::Int1, num); }
	~Uniform_int() { bgfx::destroy(handle); }
	void set(int value) { bgfx::setUniform(handle, &value, 1); }
	void set(const int *values, uint16_t num) { bgfx::setUniform(handle, values, num); }
	bgfx::UniformHandle handle;
};

struct Uniform_mat4
{
	Uniform_mat4(const char *name, uint16_t num = 1) { handle = bgfx::createUniform(name, bgfx::UniformType::Mat4, num); }
	~Uniform_mat4() { bgfx::destroy(handle); }
	void set(const mat4 &value) { bgfx::setUniform(handle, &value, 1); }
	void set(const mat4 *values, uint16_t num) { bgfx::setUniform(handle, values, num); }
	bgfx::UniformHandle handle;
};

struct Uniform_vec4
{
	Uniform_vec4(const char *name, uint16_t num = 1) { handle = bgfx::createUniform(name, bgfx::UniformType::Vec4, num); }
	~Uniform_vec4() { bgfx::destroy(handle); }
	void set(vec4 value) { bgfx::setUniform(handle, &value, 1); }
	void set(const vec4 *values, uint16_t num) { bgfx::setUniform(handle, values, num); }
	bgfx::UniformHandle handle;
};

struct Uniforms
{
	/// @remarks Only x used.
	Uniform_vec4 depthRangeEnabled = "u_DepthRangeEnabled";
	/// @remarks x is offset, y is scale, z is near z, w is far z.
	Uniform_vec4 depthRange = "u_DepthRange";

	/// @remarks Only x used.
	Uniform_vec4 renderMode = "u_RenderMode";

	/// @remarks Used by entities and the world.
	Uniform_vec4 localViewOrigin = "u_LocalViewOrigin";

	/// @remarks (1.0 / width, 1.0 / height, width, height)
	Uniform_vec4 smaaMetrics = "u_SmaaMetrics";

	/// @remarks x is soft sprite depth, y is whether to use the shader alpha, z is whether the sprite is an autosprite.
	Uniform_vec4 softSprite_Depth_UseAlpha = "u_SoftSprite_Depth_UseAlpha";

	/// @brief Used by TextureDebug fragment shader.
	/// @remarks Only x used.
	Uniform_vec4 textureDebug = "u_TextureDebug";

	Uniform_vec4 viewOrigin = "u_ViewOrigin";
	Uniform_vec4 viewUp = "u_ViewUp";

	/// @name Portal clipping
	/// @{

	/// @remarks Only x used.
	Uniform_vec4 portalClipEnabled = "u_PortalClipEnabled";

	/// World space portal plane.
	Uniform_vec4 portalPlane = "u_PortalPlane";
	/// @}

	/// @name Dynamic lights
	/// @{

	/// @remarks xyz is cell size in world coordinates, w is the texture size.
	Uniform_vec4 dynamicLightCellSize = "u_DynamicLightCellSize";

	/// @remarks w not used.
	Uniform_vec4 dynamicLightGridOffset = "u_DynamicLightGridOffset";

	/// @remarks w not used.
	Uniform_vec4 dynamicLightGridSize = "u_DynamicLightGridSize";

	/// @remarks x is the number of dynamic lights, y is the intensity scale.
	Uniform_vec4 dynamicLight_Num_Intensity = "u_DynamicLight_Num_Intensity";

	/// @remarks w not used.
	Uniform_vec4 dynamicLightTextureSizes_Cells_Indices_Lights = "u_DynamicLightTextureSizes_Cells_Indices_Lights";

	/// @}

	/// @name Fog
	/// @{

	/// @brief Enable fog in the generic shader.
	/// @remarks Only x used.
	Uniform_vec4 fogEnabled = "u_FogEnabled";

	Uniform_vec4 fogDistance = "u_FogDistance";
	Uniform_vec4 fogDepth = "u_FogDepth";

	/// @remarks Only x used.
	Uniform_vec4 fogEyeT = "u_FogEyeT";
	/// @}

	/// @name Bloom
	/// @{
	Uniform_vec4 guassianBlurDirection = "u_GuassianBlurDirection";

	/// @remarks w not used.
	Uniform_vec4 bloom_Enabled_Write_Scale = "u_Bloom_Enabled_Write_Scale";
	/// @}

	/// @name Sun light
	/// @{
	Uniform_mat4 lightModelViewProj = "u_LightModelViewProj";
	Uniform_vec4 shadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias = "u_ShadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias";
	Uniform_vec4 sunLightColor = "u_SunLightColor";
	Uniform_vec4 sunLightDir = "u_SunLightDir";
	/// @}

	/// @name Texture samplers
	/// @{

	/// General purpose sampler.
	Uniform_int textureSampler = "u_TextureSampler";

	Uniform_int bloomSampler = "u_BloomSampler";
	Uniform_int shadowMapSampler = "u_ShadowMapSampler";
	Uniform_int noiseSampler = "u_NoiseSampler";
	Uniform_int smaaColorSampler = "u_SmaaColorSampler";
	Uniform_int smaaEdgesSampler = "u_SmaaEdgesSampler";
	Uniform_int smaaAreaSampler = "u_SmaaAreaSampler";
	Uniform_int smaaSearchSampler = "u_SmaaSearchSampler";
	Uniform_int smaaBlendSampler = "u_SmaaBlendSampler";
	/// @}
};

/// @brief Uniforms derived from entity state.
struct Uniforms_Entity
{
	Uniform_vec4 ambientLight = "u_AmbientLight";
	Uniform_vec4 directedLight = "u_DirectedLight";
	Uniform_vec4 lightDirection = "u_LightDirection";
};

/// @brief Uniforms derived from material state.
struct Uniforms_Material
{
	/// @remarks Only x used.
	Uniform_vec4 time = "u_Time";

	/// @name deform gen
	/// @{

	/// @remarks Only x used.
	Uniform_vec4 nDeforms = "u_NumDeforms";

	/// @remarks Only xyz used.
	Uniform_vec4 deformMoveDirs = { "u_DeformMoveDirs", Material::maxDeforms };

	Uniform_vec4 deform_Gen_Wave_Base_Amplitude = { "u_Deform_Gen_Wave_Base_Amplitude", Material::maxDeforms };
	Uniform_vec4 deform_Frequency_Phase_Spread = { "u_Deform_Frequency_Phase_Spread", Material::maxDeforms };

	/// @}
};

/// @brief Uniforms derived from material stage state.
struct Uniforms_MaterialStage
{
	/// @remarks Only x used.
	Uniform_vec4 alphaTest = "u_AlphaTest";

	Uniform_vec4 animation_Enabled_Fraction = "u_Animation_Enabled_Fraction";

	Uniform_vec4 fogColorMask = "u_FogColorMask";

	/// @name Texture samplers
	/// @{
	Uniform_int depthSampler = "u_DepthSampler";
	Uniform_int diffuseSampler = "u_DiffuseSampler";
	Uniform_int diffuseSampler2 = "u_DiffuseSampler2";
	Uniform_int dynamicLightCellsSampler = "u_DynamicLightCellsSampler";
	Uniform_int dynamicLightIndicesSampler = "u_DynamicLightIndicesSampler";
	Uniform_int dynamicLightsSampler = "u_DynamicLightsSampler";
	Uniform_int lightSampler = "u_LightSampler";
	/// @}

	Uniform_vec4 color = "u_Color";
	Uniform_vec4 baseColor = "u_BaseColor";
	Uniform_vec4 vertexColor = "u_VertColor";
	Uniform_vec4 normalScale = "u_NormalScale";
	Uniform_vec4 specularScale = "u_SpecularScale";
	Uniform_vec4 enableTextures = "u_EnableTextures";

	/// @remarks x is the light type (MaterialLight).
	Uniform_vec4 lightType = "u_LightType";

	struct Generators
	{
		enum { Alpha = GEN_ALPHA, Color = GEN_COLOR, TexCoord = GEN_TEXCOORD };
	};

	Uniform_vec4 generators = "u_Generators";

	/// @name tcmod
	/// @{
	Uniform_vec4 diffuseTextureMatrix = "u_DiffuseTexMatrix";
	Uniform_vec4 diffuseTextureOffsetTurbulent = "u_DiffuseTexOffTurb";
	/// @}

	/// @name tcgen
	/// @{
	Uniform_vec4 tcGenVector0 = "u_TCGen0Vector0";
	Uniform_vec4 tcGenVector1 = "u_TCGen0Vector1";
	/// @}

	/// @name rgbagen
	/// @{

	/// @remarks Only x used.
	Uniform_vec4 portalRange = "u_PortalRange";
	/// @}
};

namespace util
{
	/// @name Parsing
	/// @{

	void BeginParseSession(const char *name);
	int GetCurrentParseLine();

	/*
	Parse a token out of a string
	Will never return NULL, just empty strings

	If "allowLineBreaks" is qtrue then an empty
	string will be returned if the next token is
	a newline.
	*/
	char *Parse(char **data_p, bool allowLineBreaks = true);

	/*
	The next token should be an open brace or set depth to 1 if already parsed it.
	Skips until a matching close brace is found.
	Internal brace depths are properly skipped.
	*/
	bool SkipBracedSection(char **program, int depth);

	void SkipRestOfLine(char **data);
	int Compress(char *data_p);

	/// @}

	uint16_t CalculateSmallestPowerOfTwoTextureSize(int nPixels);

	/// @brief Given a triangulated quad, extract the unique corner vertices.
	std::array<Vertex *, 4> ExtractQuadCorners(Vertex *vertices, const uint16_t *indices);

	bool IsGeometryOffscreen(const mat4 &mvp, const uint16_t *indices, size_t nIndices, const Vertex *vertices);
	bool IsGeometryBackfacing(vec3 cameraPosition, const uint16_t *indices, size_t nIndices, const Vertex *vertices, float *shortestVertexDistanceSquared = nullptr);

	vec3 MirroredPoint(const vec3 in, const Transform &surface, const Transform &camera);
	vec3 MirroredVector(const vec3 in, const Transform &surface, const Transform &camera);
	vec3 OverbrightenColor(vec3 color);
	void OverbrightenColor(const uint8_t *src, uint8_t *dest);

	char *SkipPath(char *pathname);
	const char *GetFilename(const char *name);
	const char *GetExtension(const char *name);
	void StripExtension(const char *in, char *out, int destsize);

	int Sprintf(char *dest, int size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

	// portable case insensitive compare
	int Stricmp(const char *s1, const char *s2);
	int Stricmpn(const char *s1, const char *s2, int n);

	// buffer size safe library replacements
	void Strncpyz(char *dest, const char *src, int destsize); // Safe strncpy that ensures a trailing zero
	void Strcat(char *dest, int size, const char *src); // never goes past bounds or leaves without a terminating 0

	char *ToLowerCase(char *s1);

	// does a varargs printf into a temp buffer, so I don't need to have varargs versions of all text functions.
	char *VarArgs(const char *format, ...) __attribute__((format(printf, 1, 2)));

	int Vsnprintf(char *str, size_t size, const char *format, va_list ap);

	vec3 ToGamma(vec3 color);
	vec4 ToGamma(vec4 color);
	vec3 ToLinear(vec3 color);
	vec4 ToLinear(vec4 color);
	vec4b EncodeRGBM(vec3 color);
}

struct Vertex
{
	vec3 pos;
	uint16_t normal[4];
	uint16_t texCoord[4];
	vec4b color; // Linear space.

	void setColor(vec4 c)
	{
		setColor(c.r, c.g, c.b, c.a);
	}

	void setColor(float r, float g, float b, float a)
	{
		color.r = uint8_t(std::min(r, 1.0f) * 255.0f);
		color.g = uint8_t(std::min(g, 1.0f) * 255.0f);
		color.b = uint8_t(std::min(b, 1.0f) * 255.0f);
		color.a = uint8_t(std::min(a, 1.0f) * 255.0f);
	}

	vec3 getNormal() const
	{
		return vec3
		(
			bx::bitsToFloat(half_to_float(normal[0])),
			bx::bitsToFloat(half_to_float(normal[1])),
			bx::bitsToFloat(half_to_float(normal[2]))
		);
	}

	void setNormal(vec3 n)
	{
		setNormal(n.x, n.y, n.z);
	}

	void setNormal(float x, float y, float z)
	{
		normal[0] = half_from_float(bx::floatToBits(x));
		normal[1] = half_from_float(bx::floatToBits(y));
		normal[2] = half_from_float(bx::floatToBits(z));
	}

	vec4 getTexCoord() const
	{
		return vec4
		(
			bx::bitsToFloat(half_to_float(texCoord[0])),
			bx::bitsToFloat(half_to_float(texCoord[1])),
			bx::bitsToFloat(half_to_float(texCoord[2])),
			bx::bitsToFloat(half_to_float(texCoord[3]))
		);
	}

	void setTexCoord(vec2 st)
	{
		setTexCoord(st.x, st.y);
	}

	void setTexCoord(vec4 stuv)
	{
		setTexCoord(stuv.x, stuv.y, stuv.z, stuv.w);
	}

	void setTexCoord(float s, float t, float u = 0.0f, float v = 0.0f)
	{
		texCoord[0] = half_from_float(bx::floatToBits(s));
		texCoord[1] = half_from_float(bx::floatToBits(t));
		texCoord[2] = half_from_float(bx::floatToBits(u));
		texCoord[3] = half_from_float(bx::floatToBits(v));
	}

	static void init()
	{
		decl.begin();
		decl.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
		decl.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Half);
		decl.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Half);
		decl.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
		decl.m_stride = sizeof(Vertex);
		decl.m_offset[bgfx::Attrib::Position] = offsetof(Vertex, pos);
		decl.m_offset[bgfx::Attrib::Normal] = offsetof(Vertex, normal);
		decl.m_offset[bgfx::Attrib::TexCoord0] = offsetof(Vertex, texCoord);
		decl.m_offset[bgfx::Attrib::Color0] = offsetof(Vertex, color);
		decl.end();
	}

	static bgfx::VertexDecl decl;
};

struct VertexBuffer
{
	VertexBuffer() { handle.idx = bgfx::kInvalidHandle; }
	~VertexBuffer() { if (bgfx::isValid(handle)) bgfx::destroy(handle); }
	bgfx::VertexBufferHandle handle;
};

enum class VisibilityId
{
	None, // non-world scenes
	Main,
	Portal,
	Probe,
	Reflection,
	SkyboxPortal,
	Num
};

struct WarnOnceId
{
	enum Enum
	{
		TransientBuffer,
		Num
	};
};

void WarnOnce(WarnOnceId::Enum id);

namespace window
{
	float GetAspectRatio();
	int GetRefreshRate();
	int GetWidth();
	int GetHeight();
	int IsFullscreen();
	void Initialize();
	void SetGamma(const uint8_t *red, const uint8_t *green, const uint8_t *blue);
	void Shutdown();
}

namespace world
{
	void Load(const char *name);
	void Unload();
	bool IsLoaded();
	vec2i GetLightmapSize();
	int GetNumLightmaps();
	Texture *GetLightmap(int index);
	bool GetEntityToken(char *buffer, int size);
	bool HasLightGrid();
	void SampleLightGrid(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir);
	bool InPvs(vec3 position);
	bool InPvs(vec3 position1, vec3 position2);
	int FindFogIndex(vec3 position, float radius);
	int FindFogIndex(const Bounds &bounds);
	void CalculateFog(int fogIndex, const mat4 &modelMatrix, const mat4 &modelViewMatrix, vec3 cameraPosition, vec3 localViewPosition, const mat3 &cameraRotation, vec4 *fogColor, vec4 *fogDistance, vec4 *fogDepth, float *eyeT);
	int MarkFragments(int numPoints, const vec3 *points, vec3 projection, int maxPoints, vec3 *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);
	Bounds GetBounds();
	Bounds GetBounds(VisibilityId visId);
	size_t GetNumSkySurfaces(VisibilityId visId);
	const SkySurface &GetSkySurface(VisibilityId visId, size_t index);
	bool CalculatePortalCamera(VisibilityId visId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, const std::vector<renderer::Entity> &entities, vec3 *pvsPosition, Transform *portalCamera, bool *isMirror, Plane *portalPlane);
	bool CalculateReflectionCamera(VisibilityId visId, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, Transform *camera, Plane *plane);
	void RenderPortal(VisibilityId visId, DrawCallList *drawCallList);
	void RenderReflective(VisibilityId visId, DrawCallList *drawCallList);
	void UpdateVisibility(VisibilityId visId, vec3 cameraPosition, const uint8_t *areaMask);
	void Render(VisibilityId visId, DrawCallList *drawCallList, const mat3 &sceneRotation);
	void PickMaterial();
}

static const size_t g_funcTableSize = 1024;
static const size_t g_funcTableSize2 = 10;
static const size_t g_funcTableMask = g_funcTableSize - 1;
static const float g_overbrightFactor = 2.0f;
static const float g_identityLight = 1.0f / g_overbrightFactor;
static const size_t g_gammaTableSize = 256;

extern uint8_t g_gammaTable[g_gammaTableSize];
extern bool g_hardwareGammaEnabled;
extern ConsoleVariables g_cvars;
extern const uint8_t *g_externalVisData;
extern MaterialCache *g_materialCache;
extern ModelCache *g_modelCache;
extern TextureCache *g_textureCache;

extern float g_sinTable[g_funcTableSize];
extern float g_squareTable[g_funcTableSize];
extern float g_triangleTable[g_funcTableSize];
extern float g_sawToothTable[g_funcTableSize];
extern float g_inverseSawToothTable[g_funcTableSize];

} // namespace renderer
