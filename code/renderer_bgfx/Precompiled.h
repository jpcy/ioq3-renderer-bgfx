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

#include <algorithm>
#include <memory>
#include <vector>

extern "C"
{
#ifdef USE_LOCAL_HEADERS
#include "SDL.h"
#include "SDL_syswm.h"
#else
#include <SDL.h>
#include <SDL_syswm.h>
#endif

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
}

#include "../math/Math.h"
using namespace math;

#include "bgfx/bgfx.h"
#include "bgfx/bgfxplatform.h"
#include "bx/debug.h"
#include "bx/fpumath.h"
#include "bx/string.h"

#define BIT(x) (1<<(x))

namespace renderer {

extern refimport_t ri;
extern glconfig_t glConfig;

struct Entity;
class Material;
class Skin;
class Texture;
struct Vertex;

struct BgfxCallback : bgfx::CallbackI
{
	void fatal(bgfx::Fatal::Enum _code, const char* _str) override;
	void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override;
	uint32_t cacheReadSize(uint64_t _id) override;
	bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override;
	void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override;
	void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t _size, bool _yflip) override;
	void captureBegin(uint32_t _width, uint32_t _height, uint32_t _pitch, bgfx::TextureFormat::Enum _format, bool _yflip) override;
	void captureEnd() override;
	void captureFrame(const void* _data, uint32_t _size) override;

private:
	std::vector<uint8_t> screenShotDataBuffer_;
	std::vector<uint8_t> screenShotFileBuffer_;
};

struct ConsoleVariables
{
	ConsoleVariables();

	cvar_t *backend;
	cvar_t *mode;
	cvar_t *fullscreen;
	cvar_t *noborder;
	cvar_t *customwidth;
	cvar_t *customheight;
	cvar_t *customPixelAspect;
	cvar_t *ignorehwgamma;
	cvar_t *allowResize;
	cvar_t *centerWindow;
	cvar_t *maxAnisotropy;
	cvar_t *msaa;
	cvar_t *picmip;
	cvar_t *screenshotJpegQuality;
	cvar_t *wireframe;
	cvar_t *bgfx_stats;
	cvar_t *debugText;
	cvar_t *normalMapping;
	cvar_t *specularMapping;
	cvar_t *deluxeMapping;
	cvar_t *deluxeSpecular;
	cvar_t *specularIsMetallic;
	cvar_t *overBrightBits;
	cvar_t *ambientScale;
	cvar_t *directedScale;

	/// @name Railgun
	/// @{
	cvar_t *railWidth;
	cvar_t *railCoreWidth;
	cvar_t *railSegmentLength;
	/// @}
};

struct DrawCallFlags
{
	enum
	{
		None,
		SkyboxSideFirst,
		SkyboxSideLast = SkyboxSideFirst + 5
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

		/// @remarks Not used by dynamic buffers.
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

	const Entity *entity = nullptr;
	int flags = DrawCallFlags::None;
	int fogIndex = -1;
	IndexBuffer ib;
	Material *material = nullptr;
	mat4 modelMatrix = mat4::identity;
	uint64_t state = BGFX_STATE_RGB_WRITE | BGFX_STATE_ALPHA_WRITE | BGFX_STATE_MSAA;
	VertexBuffer vb;
	float zOffset = 0.0f;
	float zScale = 1.0f;
};

typedef std::vector<DrawCall> DrawCallList;

struct DynamicIndexBuffer
{
	DynamicIndexBuffer() { handle.idx = bgfx::invalidHandle; }
	~DynamicIndexBuffer() { if (bgfx::isValid(handle)) bgfx::destroyDynamicIndexBuffer(handle); }
	bgfx::DynamicIndexBufferHandle handle;
};

struct DynamicLight
{
	static const size_t max = 32;
	vec4 color;
	float intensity;
	vec3 position;
};

struct Entity
{
	refEntity_t e;

	vec3 localViewPosition;

	/// Normalized direction towards light, in world space.
	vec3 lightDir;

	/// Normalized direction towards light, in model space.
	vec3 modelLightDir;

	/// Color normalized to 0-255.
	vec3 ambientLight;

	/// 32-bit RGBA packed.
	int ambientLightInt;

	vec3 directedLight;
};

/// @brief Given a triangulated quad, extract the unique corner vertices.
std::array<Vertex *, 4> ExtractQuadCorners(Vertex *vertices, const uint16_t *indices);

struct Image
{
	Image() {}
	Image(const char *filename, int flags = 0);
	void calculateNumMips();
	void allocMemory();

	struct Flags
	{
		enum
		{
			GenerateMipmaps = BIT(0),
			Picmip          = BIT(1)
		};
	};

	const bgfx::Memory *memory = nullptr;
	int width = 0;
	int height = 0;
	int nComponents = 0;
	int nMips = 1;
};

struct IndexBuffer
{
	IndexBuffer() { handle.idx = bgfx::invalidHandle; }
	~IndexBuffer() { if (bgfx::isValid(handle)) bgfx::destroyIndexBuffer(handle); }
	bgfx::IndexBufferHandle handle;
};

enum class MaterialAdjustColorsForFog
{
	None,
	ModulateRGB,
	ModulateRGBA,
	ModulateAlpha
};

enum class MaterialAlphaGen
{
	Identity,
	Skip,
	Entity,
	OneMinusEntity,
	Vertex,
	OneMinusVertex,
	LightingSpecular,
	Waveform,
	Portal,
	Const,
};

enum class MaterialAlphaTest
{
	None,
	GT_0,
	LT_128,
	GE_128
};

enum class MaterialColorGen
{
	Bad,
	IdentityLighting,	// Main::identityLight
	Identity,			// always (1,1,1,1)
	Entity,			// grabbed from entity's modulate field
	OneMinusEntity,	// grabbed from 1 - entity.modulate
	ExactVertex,		// tess.vertexColors
	Vertex,			// tess.vertexColors * Main::identityLight
	ExactVertexLit,	// like ExactVertex but takes a light direction from the lightgrid
	VertexLit,		// like Vertex but takes a light direction from the lightgrid
	OneMinusVertex,
	Waveform,			// programmatically generated
	LightingDiffuse,
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
	None,
	Wave,
	Normals,
	Bulge,
	Move,
	ProjectionShadow,
	Autosprite,
	Autosprite2,
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
	None,
	Sin,
	Square,
	Triangle,
	Sawtooth,
	InverseSawtooth,
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
	None,
	Map,
	Vertex,
	Vector
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

enum class MaterialTexCoordGen
{
	None,

	/// Clear to 0,0
	Identity,

	Lightmap,
	Texture,
	EnvironmentMapped,
	Fog,

	/// S and T from world coordinates.
	Vector
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
/// @remarks Sync with shaders.
struct MaterialTextureBundleIndex
{
	enum
	{
		DiffuseMap,
		Lightmap,
		NormalMap,
		Deluxemap,
		Specularmap,
		NumMaterialTextureBundles,

		ColorMap  = 0,
		Levelsmap = 1
	};
};

struct MaterialStage
{
	bool active = false;

	MaterialTextureBundle bundles[MaterialTextureBundleIndex::NumMaterialTextureBundles];

	MaterialWaveForm rgbWave;
	MaterialColorGen rgbGen = MaterialColorGen::Bad;

	MaterialWaveForm alphaWave;
	MaterialAlphaGen alphaGen = MaterialAlphaGen::Identity;

	uint8_t constantColor[4];			// for MaterialColorGen::Const and MaterialAlphaGen::Const

	uint64_t depthTestBits = BGFX_STATE_DEPTH_TEST_LEQUAL;

	bool depthWrite = true;

	MaterialAlphaTest alphaTest = MaterialAlphaTest::None;

	uint64_t blendSrc = 0;
	uint64_t blendDst = 0;

	MaterialAdjustColorsForFog adjustColorsForFog = MaterialAdjustColorsForFog::None;

	bool isDetail = false;

	MaterialStageType type = MaterialStageType::ColorMap;
	MaterialLight light = MaterialLight::None;

	vec4 normalScale;
	vec4 specularScale;
};

class Material
{
public:
	Material(const char *name);
	size_t getNumStages() const { return numUnfoggedPasses; }
	int32_t getDepth() const { return (int32_t)sort; }

	/// @name deforms
	/// @{

	int getNumGpuDeforms() const;
	bool hasCpuDeforms() const;
	bool hasGpuDeforms() const;
	bool isCpuDeform(MaterialDeform deform) const;
	bool isGpuDeform(MaterialDeform deform) const;

	/// @}

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

	unsigned int surfaceFlags = 0;			// if explicitlyDefined, this will have SURF_* flags
	unsigned int contentFlags = 0;

	bool entityMergable = false;			// merge across entites optimizable (smoke, blood)

	bool isSky = false;
	MaterialSkyParms sky;
	MaterialFogParms fogParms;

	float portalRange = 256;			// distance to fog out at
	bool isPortal = false;

	MaterialCullType cullType = MaterialCullType::FrontSided;				// MaterialCullType::FrontSided, MaterialCullType::BackSided, or MaterialCullType::TwoSided
	bool polygonOffset = false;			// set for decals and other items that must be offset
	bool noMipMaps = false;				// for console fonts, 2D elements, etc.
	bool noPicMip = false;				// for images that must always be full resolution

	MaterialFogPass fogPass = MaterialFogPass::None;				// draw a blended pass, possibly with depth test equals

	int vertexAttribs = 0;          // not all shaders will need all data to be gathered

	int numDeforms = 0;
	static const size_t maxDeforms = 3;
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
	void setTime(float time);
	void doCpuDeforms(DrawCall *dc) const;
	void setStageShaderUniforms(size_t stageIndex) const;
	void setFogShaderUniforms() const;
	void setStageTextureSamplers(size_t stageIndex) const;
	vec4 calculateStageFogColorMask(size_t stageIndex) const;
	uint64_t calculateStageState(size_t stageIndex, uint64_t state) const;
	bgfx::ProgramHandle calculateStageShaderProgramHandle(size_t stageIndex) const;

private:
	void setStageTextureSampler(size_t stageIndex, int sampler) const;
	float *tableForFunc(MaterialWaveformGenFunc func) const;
	float evaluateWaveForm(const MaterialWaveForm &wf) const;
	float evaluateWaveFormClamped(const MaterialWaveForm &wf) const;
	void calculateTexMods(const MaterialStage &stage, vec4 *outMatrix, vec4 *outOffTurb) const;
	void calculateTurbulentFactors(const MaterialWaveForm &wf, float *amplitude, float *now) const;
	void calculateScaleTexMatrix(vec2 scale, float *matrix) const;
	void calculateScrollTexMatrix(vec2 scrollSpeed, float *matrix) const;
	void calculateStretchTexMatrix(const MaterialWaveForm &wf, float *matrix) const;
	void calculateTransformTexMatrix(const MaterialTexModInfo &tmi, float *matrix) const;
	void calculateRotateTexMatrix(float degsPerSecond, float *matrix) const;

	float calculateWaveColorSingle(const MaterialWaveForm &wf) const;
	float calculateWaveAlphaSingle(const MaterialWaveForm &wf) const;

	/// rgbGen and alphaGen
	void calculateColors(const MaterialStage &stage, vec4 *baseColor, vec4 *vertColor) const;

	void setDeformUniforms() const;

	/// @remarks Set when precalculate() is called.
	float time_;

	/// @}

	friend class MaterialCache;
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

	Skin *findSkin(const char *name);
	Skin *getSkin(qhandle_t handle) { return skins_[handle].get(); }

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
	char *s_shaderText;

	std::vector<std::unique_ptr<Material>> materials_;

	static const size_t hashTableSize_ = 1024;
	Material *hashTable_[hashTableSize_];

	static const size_t textHashTableSize_ = 2048;
	char **textHashTable_[textHashTableSize_];

	Material *defaultMaterial_;

	std::vector<std::unique_ptr<Skin>> skins_;
};

class Model
{
public:
	virtual ~Model() {}
	virtual bool load() = 0;
	virtual Bounds getBounds() const = 0;
	virtual Transform getTag(const char *name, int frame) const = 0;
	virtual void render(DrawCallList *drawCallList, Entity *entity) = 0;
	size_t getIndex() const { return index_; }
	const char *getName() const { return name_; }

	static std::unique_ptr<Model> createMD3(const char *name);

protected:
	char name_[MAX_QPATH];

private:
	size_t index_;
	Model *next_ = NULL;

	friend class ModelCache;
};

class ModelCache
{
public:
	ModelCache();
	Model *findModel(const char *name);
	Model *addModel(std::unique_ptr<Model> model);
	Model *getModel(int handle) { return models_[handle].get(); }

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
	vec3_t			cullBounds[2];
	vec3_t			cullOrigin;
	float			cullRadius;
	cplane_t        cullPlane;

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
	vec3_t			lodOrigin;
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

class ReadOnlyFile
{
public:
	ReadOnlyFile(const char *filename) { length_ = ri.FS_ReadFile(filename, (void **)&data_); }
	~ReadOnlyFile() { if (data_) ri.FS_FreeFile((void *)data_); }
	bool isValid() const { return data_ && length_ >= 0; }
	const uint8_t *getData() const { return data_; }
	uint8_t *getData() { return data_; }
	size_t getLength() const { return (size_t)length_; }

private:
	uint8_t *data_;
	long length_;
};

struct Shader
{
	Shader() { handle.idx = bgfx::invalidHandle; }
	~Shader() { if (bgfx::isValid(handle)) bgfx::destroyShader(handle); }
	bgfx::ShaderHandle handle;
};

struct ShaderProgram
{
	ShaderProgram() { handle.idx = bgfx::invalidHandle; }
	~ShaderProgram() { if (bgfx::isValid(handle)) bgfx::destroyProgram(handle); }
	bgfx::ProgramHandle handle;
};

enum class ShaderProgramId
{
	Fog,
	Generic,
	TextureColor
};

class ShaderCache
{
public:
	struct GenericPermutations
	{
		enum
		{
			AlphaTest = BIT(0),
			Count     = BIT(1),
			All       = Count - 1
		};
	};

	ShaderCache();
	void initialize();

	struct GetHandleFlags
	{
		enum
		{
			ReturnInvalid = BIT(0)
		};
	};

	bgfx::ProgramHandle getHandle(ShaderProgramId program, int programIndex = 0, int flags = 0) const;

private:
	struct Bundle
	{
		Shader vertex;
		Shader fragment;
		ShaderProgram program;
	};

	bool createBundle(Bundle *bundle, const char *name, const char *vertexDefines, const char *fragmentDefines, size_t vertexPermutationIndex = 0, size_t fragmentPermutationIndex = 0);

	char constants_[1024];
	Bundle fog_, generic_[GenericPermutations::Count], textureColor_;
};

class Skin
{
public:
	/// Create a skin by loading it from a file.
	Skin(const char *name, qhandle_t handle);

	/// Create a skin with a single surface using the supplied material.
	Skin(const char *name, qhandle_t handle, Material *material);

	bool hasSurfaces() const { return nSurfaces_ > 0; }
	const char *getName() const { return name_; }
	qhandle_t getHandle() const { return handle_; }

	Material *findMaterial(const char *surfaceName);

private:
	struct Surface
	{
		char name[MAX_QPATH];
		Material *material;
	};

	/// Game path, including extension.
	char name_[MAX_QPATH];

	qhandle_t handle_;
	Surface surfaces_[MD3_MAX_SURFACES];
	size_t nSurfaces_;
};

/// @remarks Called when a sky material is parsed.
void Sky_InitializeTexCoords(float heightCloud);

void Sky_Render(DrawCallList *drawCallList, vec3 viewPosition, uint8_t visCacheId, float zMax);

enum class TextureType
{
	/// For color, lightmap, diffuse, and specular.
	ColorAlpha,

	Normal,
	NormalHeight,

	/// Normals are swizzled, deluxe are not.
	Deluxe
};

struct TextureFlags
{
	enum
	{
		None           = 0x0000,
		Mipmap         = 0x0001,
		Picmip         = 0x0002,
		Cubemap        = 0x0004,
		NoCompression = 0x0010,
		NoLightScale   = 0x0020,
		ClampToEdge    = 0x0040,
		SRGB           = 0x0080,
		GenNormalMap   = 0x0100,
	};
};

class Texture
{
public:
	Texture(const char *name, const Image &image, TextureType type, int flags, bgfx::TextureFormat::Enum format);
	~Texture();
	void setSampler(int sampler) const;
	void resize(int width, int height);
	void update(const bgfx::Memory *mem, int x, int y, int width, int height);
	bgfx::TextureHandle getHandle() const { return handle_; }
	int getWidth() const { return width_; }
	int getHeight() const { return height_; }

private:
	uint32_t calculateBgfxFlags() const;

	char name_[MAX_QPATH];
	TextureType type_;
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
	Texture *createTexture(const char *name, const Image &image, TextureType type = TextureType::ColorAlpha, int flags = TextureFlags::None, bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8);

	/// Finds or loads the given image.
	/// @return nullptr if it fails, not the default image.
	Texture *findTexture(const char *name, TextureType type = TextureType::ColorAlpha, int flags = TextureFlags::None);

	const Texture *getDefaultTexture() const { return defaultTexture_; }
	const Texture *getWhiteTexture() const { return whiteTexture_; }
	std::array<Texture *, 32> &getScratchTextures() { return scratchTextures_; }

private:
	void createBuiltinTextures();
	static size_t generateHash(const char *name);

	std::vector<std::unique_ptr<Texture>> textures_;
	static const size_t hashTableSize_ = 1024;
	Texture *hashTable_[hashTableSize_];

	Texture *defaultTexture_, *identityLightTexture_, *whiteTexture_;
	std::array<Texture *, 32> scratchTextures_;
};

struct Uniform_int
{
	Uniform_int(const char *name, uint16_t num = 1) { handle = bgfx::createUniform(name, bgfx::UniformType::Int1, num); }
	~Uniform_int() { bgfx::destroyUniform(handle); }
	void set(int value) { bgfx::setUniform(handle, &value, 1); }
	void set(const int *values, uint16_t num) { bgfx::setUniform(handle, values, num); }
	bgfx::UniformHandle handle;
};

struct Uniform_vec4
{
	Uniform_vec4(const char *name, uint16_t num = 1) { handle = bgfx::createUniform(name, bgfx::UniformType::Vec4, num); }
	~Uniform_vec4() { bgfx::destroyUniform(handle); }
	void set(vec4 value) { bgfx::setUniform(handle, &value, 1); }
	void set(const vec4 *values, uint16_t num) { bgfx::setUniform(handle, values, num); }
	bgfx::UniformHandle handle;
};

struct Uniforms
{
	Uniforms()
	{
		textures[MaterialTextureBundleIndex::DiffuseMap] = &diffuseMap;
		textures[MaterialTextureBundleIndex::Lightmap] = &lightmap;
		textures[MaterialTextureBundleIndex::NormalMap] = &normalMap;
		textures[MaterialTextureBundleIndex::Deluxemap] = &deluxemap;
		textures[MaterialTextureBundleIndex::Specularmap] = &specularmap;
	}

	/// @name Texture samplers
	/// @{
	Uniform_int diffuseMap = "u_DiffuseMap";
	Uniform_int lightmap = "u_LightMap";
	Uniform_int normalMap = "u_NormalMap";
	Uniform_int deluxemap = "u_DeluxeMap";
	Uniform_int specularmap = "u_SpecularMap";
	Uniform_int *textures[MaterialTextureBundleIndex::NumMaterialTextureBundles];
	/// @}

	Uniform_vec4 color = "u_Color";
	Uniform_vec4 baseColor = "u_BaseColor";
	Uniform_vec4 vertexColor = "u_VertColor";
	Uniform_vec4 normalScale = "u_NormalScale";
	Uniform_vec4 specularScale = "u_SpecularScale";
	Uniform_vec4 enableTextures = "u_EnableTextures";

	Uniform_vec4 viewOrigin = "u_ViewOrigin";
	Uniform_vec4 localViewOrigin = "u_LocalViewOrigin";

	/// @remarks x is offset, y is scale.
	Uniform_vec4 depthRange = "u_DepthRange";

	/// @remarks Only x used.
	Uniform_vec4 portalClip = "u_PortalClip";

	/// World space portal plane.
	Uniform_vec4 portalPlane = "u_PortalPlane";

	/// @remarks Only x used.
	Uniform_vec4 lightType = "u_LightType";

	struct Generators
	{
		enum { TexCoord, Color, Alpha };
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
	Uniform_vec4 ambientLight = "u_AmbientLight";
	Uniform_vec4 directedLight = "u_DirectedLight";
	Uniform_vec4 lightDirection = "u_LightDirection";

	/// @remarks Only x used.
	Uniform_vec4 lightRadius = "u_LightRadius";
	
	Uniform_vec4 modelLightDir = "u_ModelLightDir";

	/// @remarks Only x used.
	Uniform_vec4 portalRange = "u_PortalRange";
	/// @}

	/// @name fog
	/// @{

	/// @remarks Only x used.
	Uniform_vec4 fogEnabled = "u_FogEnabled";

	Uniform_vec4 fogDistance = "u_FogDistance";
	Uniform_vec4 fogDepth = "u_FogDepth";

	/// @remarks Only x used.
	Uniform_vec4 fogEyeT = "u_FogEyeT";

	Uniform_vec4 fogColorMask = "u_FogColorMask";
	/// @}

	/// @name deform gen
	/// @{

	/// @remarks Only x used.
	Uniform_vec4 nDeforms = "u_NumDeforms";

	/// @remarks Only xyz used.
	Uniform_vec4 deformMoveDirs = { "u_DeformMoveDirs", Material::maxDeforms };

	Uniform_vec4 deform_Gen_Wave_Base_Amplitude = { "u_Deform_Gen_Wave_Base_Amplitude", Material::maxDeforms };
	Uniform_vec4 deform_Frequency_Phase_Spread = { "u_Deform_Frequency_Phase_Spread", Material::maxDeforms };

	/// @}

	/// @name alpha test
	/// @{

	Uniform_vec4 alphaTest = "u_AlphaTest";

	/// @}

	/// @remarks Only x used.
	Uniform_vec4 time = "u_Time";

	/// @remarks Only x used.
	Uniform_vec4 nDynamicLights = "u_NumDynamicLights";

	/// @remarks w is intensity.
	Uniform_vec4 dlightColors = { "u_DynamicLightColors", DynamicLight::max };

	Uniform_vec4 dlightPositions = { "u_DynamicLightPositions", DynamicLight::max };
};

struct Vertex
{
	vec3 pos;
	vec3 normal;
	vec2 texCoord;
	vec2 texCoord2;
	vec4 color;

	static void init()
	{
		decl.begin();
		decl.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
		decl.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float);
		decl.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
		decl.add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float);
		decl.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float);
		decl.m_stride = sizeof(Vertex);
		decl.m_offset[bgfx::Attrib::Position] = offsetof(Vertex, pos);
		decl.m_offset[bgfx::Attrib::Normal] = offsetof(Vertex, normal);
		decl.m_offset[bgfx::Attrib::TexCoord0] = offsetof(Vertex, texCoord);
		decl.m_offset[bgfx::Attrib::TexCoord1] = offsetof(Vertex, texCoord2);
		decl.m_offset[bgfx::Attrib::Color0] = offsetof(Vertex, color);
		decl.end();
	}

	static bgfx::VertexDecl decl;
};

struct VertexBuffer
{
	VertexBuffer() { handle.idx = bgfx::invalidHandle; }
	~VertexBuffer() { if (bgfx::isValid(handle)) bgfx::destroyVertexBuffer(handle); }
	bgfx::VertexBufferHandle handle;
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
void Window_Initialize(bool gl);
void Window_Shutdown();

class World
{
public:
	virtual ~World() = 0;
	virtual const Texture *getLightmap(size_t index) const = 0;
	virtual bool getEntityToken(char *buffer, int size) = 0;
	virtual bool hasLightGrid() const = 0;
	virtual void sampleLightGrid(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir) const = 0;
	virtual int findFogIndex(vec3 position, float radius) const = 0;
	virtual int findFogIndex(const Bounds &bounds) const = 0;
	virtual void calculateFog(int fogIndex, const mat4 &modelMatrix, const mat4 &modelViewMatrix, vec3 localViewPosition, vec4 *fogColor, vec4 *fogDistance, vec4 *fogDepth, float *eyeT) const = 0;
	virtual int markFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer) = 0;
	virtual Bounds getBounds(uint8_t visCacheId) const = 0;
	virtual Material *getSkyMaterial(uint8_t visCacheId) const = 0;
	virtual const std::vector<Vertex> &getSkyVertices(uint8_t visCacheId) const = 0;
	virtual size_t getNumPortalSurfaces(uint8_t visCacheId) const = 0;
	virtual bool calculatePortalCamera(uint8_t visCacheId, size_t portalSurfaceIndex, vec3 mainCameraPosition, mat3 mainCameraRotation, const mat4 &mvp, const std::vector<Entity> &entities, vec3 *pvsPosition, Transform *portalCamera, bool *isMirror, vec4 *portalPlane) const = 0;
	virtual void load() = 0;
	virtual uint8_t createVisCache() = 0;
	virtual void updateVisCache(uint8_t visCacheId, vec3 cameraPosition, const uint8_t *areaMask) = 0;
	virtual void render(DrawCallList *drawCallList, uint8_t visCacheId) = 0;

	static std::unique_ptr<World> createBSP(const char *name);
};

inline World::~World() {}

class Main
{
public:
	Main();
	~Main();
	void initialize();
	void registerFont(const char *fontName, int pointSize, fontInfo_t *font);
	int getTime() const { return time_; }
	float getFloatTime() const { return floatTime_; }
	int getFrameNo() const { return frameNo_; }
	void setColor(const vec4 &c) { stretchPicColor_ = c; }
	void debugPrint(const char *format, ...);
	void drawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex);
	void drawStretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, bool dirty);
	void uploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, bool dirty);
	void loadWorld(const char *name);
	void addDynamicLightToScene(DynamicLight light);
	void addEntityToScene(const refEntity_t *entity);
	void addPolyToScene(qhandle_t hShader, int nVerts, const polyVert_t *verts, int nPolys);
	void renderScene(const refdef_t *def);
	void endFrame();
	bool sampleLight(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir);

	bool sunShadows = false;
	vec3 sunLight = { 0, 0, 0 };
	vec3 sunDirection = { 0.45f, 0.3f, 0.9f };
	float mapLightScale = 1;
	float sunShadowScale = 0.5f;

	float identityLight; // 1.0 / ( 1 << overbrightBits )
	int identityLightByte; // identityLight * 255
	int overbrightBits; // r_overBrightBits->integer, but set to 0 if no hw gamma

	const Entity *currentEntity = nullptr;

	vec3 scenePosition;
	mat3 sceneRotation;

	/// Is the current camera in the world (not RDF_NOWORLDMODEL).
	bool isWorldCamera;

	/// Equivalent to scenePosition, unless rendering a portal/mirror view.
	vec3 cameraPosition;

	/// Equivalent to sceneRotation, unless rendering a portal/mirror view.
	mat3 cameraRotation;

	bool isMirrorCamera = false;
	vec4 portalPlane;

	vec2 autoExposureMinMax = { -2, 2 };
	vec3 toneMinAvgMaxLevel = { -8, -2, 0 };

	ConsoleVariables cvars;
	std::unique_ptr<Uniforms> uniforms;
	std::unique_ptr<TextureCache> textureCache;
	std::unique_ptr<ShaderCache> shaderCache;
	std::unique_ptr<MaterialCache> materialCache;
	std::unique_ptr<ModelCache> modelCache;

	static const size_t funcTableSize = 1024;
	static const size_t funcTableSize2 = 10;
	static const size_t funcTableMask = funcTableSize - 1;

	float sinTable[funcTableSize];
	float squareTable[funcTableSize];
	float triangleTable[funcTableSize];
	float sawToothTable[funcTableSize];
	float inverseSawToothTable[funcTableSize];

	/// Convert from our coordinate system (looking down X) to OpenGL's coordinate system (looking down -Z)
	static const mat4 toOpenGlMatrix;

	std::unique_ptr<World> world;
	const uint8_t *externalVisData = nullptr;

private:
	struct ViewFlags
	{
		enum
		{
			None = 0,
			ClearDepth = BIT(0),

			/// Ignores rect and matrices; sets up a fullscreen orthographic view.
			Ortho = BIT(1)
		};
	};

	uint8_t pushView(int flags = ViewFlags::None, vec4 rect = vec4::empty, const mat4 &viewMatrix = mat4::identity, const mat4 &projectionMatrix = mat4::identity);
	void flushStretchPics();
	void renderCamera(uint8_t visCacheId, vec3 pvsPosition, vec3 position, mat3 rotation, vec4 rect, vec2 fov, const uint8_t *areaMask);
	void renderEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity);
	void renderLightningEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity);
	void renderQuad(DrawCallList *drawCallList, vec3 position, vec3 normal, vec3 left, vec3 up, Material *mat, vec4 color, Entity *entity);
	void renderRailCoreEntity(DrawCallList *drawCallList, vec3 viewPosition, mat3 viewRotation, Entity *entity);
	void renderRailCore(DrawCallList *drawCallList, vec3 start, vec3 end, vec3 up, float length, float spanWidth, Material *mat, vec4 color, Entity *entity);
	void renderRailRingsEntity(DrawCallList *drawCallList, Entity *entity);
	void renderSpriteEntity(DrawCallList *drawCallList, mat3 viewRotation, Entity *entity);
	void setupEntityLighting(Entity *entity);

	static const int maxFonts_ = 6;
	int nFonts_ = 0;
	fontInfo_t fonts_[maxFonts_];

	int time_ = 0;
	float floatTime_ = 0;

	/// Incremented everytime endFrame() is called
	int frameNo_ = 0;

	uint16_t debugTextY = 0;
	vec4 stretchPicColor_;
	Material *stretchPicMaterial_ = nullptr;
	std::vector<Vertex> stretchPicVertices_;
	std::vector<uint16_t> stretchPicIndices_;
	uint8_t mainVisCacheId, portalVisCacheId;
	std::vector<Entity> sceneEntities_;

	struct Polygon
	{
		Material *material;
		int fogIndex;
		size_t firstVertex, nVertices;
	};

	std::vector<DynamicLight> sceneDynamicLights_;
	std::vector<Polygon> scenePolygons_;
	std::vector<Polygon *> sortedScenePolygons_;
	std::vector<polyVert_t> scenePolygonVertices_;
	DrawCallList drawCalls_;
	VertexBuffer fsVertexBuffer_;
	IndexBuffer fsIndexBuffer_;

	/// @remarks Resets to 0 every frame.
	uint8_t firstFreeViewId_ = 0;
};

extern std::unique_ptr<Main> g_main;

} // namespace renderer
