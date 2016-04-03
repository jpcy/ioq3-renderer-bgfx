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

#include "Main.h"

namespace renderer {

// Pull into the renderer namespace.
#include "../../build/Shader.cpp"

struct BackendMap
{
	bgfx::RendererType::Enum type;
	const char *id; // Used by r_backend cvar.
};

static const std::array<const BackendMap, 5> backendMaps =
{{
	{ bgfx::RendererType::Null, "null" },
	{ bgfx::RendererType::Direct3D11, "d3d11" },
	{ bgfx::RendererType::Direct3D12, "d3d12" },
	{ bgfx::RendererType::OpenGL, "gl" },
	{ bgfx::RendererType::Vulkan, "vulkan" }
}};

BgfxCallback bgfxCallback;

const mat4 Main::toOpenGlMatrix_
(
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
);

refimport_t ri;

bgfx::VertexDecl Vertex::decl;

static std::unique_ptr<Main> s_main;

ConsoleVariables g_cvars;
const uint8_t *g_externalVisData = nullptr;
MaterialCache *g_materialCache = nullptr;
ModelCache *g_modelCache = nullptr;

float g_sinTable[g_funcTableSize];
float g_squareTable[g_funcTableSize];
float g_triangleTable[g_funcTableSize];
float g_sawToothTable[g_funcTableSize];
float g_inverseSawToothTable[g_funcTableSize];

void ConsoleVariables::initialize()
{
	aa = ri.Cvar_Get("r_aa", "", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_SetDescription(aa,
		"<empty>   None\n"
		"fxaa      FXAA v2\n"
		"msaa2x    MSAA 2x\n"
		"msaa4x    MSAA 4x\n"
		"msaa8x    MSAA 8x\n"
		"msaa16x   MSAA 16x\n"
		"smaa      SMAA 1x\n");
	aa_hud = ri.Cvar_Get("r_aa_hud", "", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_SetDescription(aa_hud,
		"<empty>   None\n"
		"msaa2x    MSAA 2x\n"
		"msaa4x    MSAA 4x\n"
		"msaa8x    MSAA 8x\n"
		"msaa16x   MSAA 16x\n");
	backend = ri.Cvar_Get("r_backend", "", CVAR_ARCHIVE | CVAR_LATCH);

	{
		// Have the r_backend cvar print a list of supported backends when invoked without any arguments.
		bgfx::RendererType::Enum supportedBackends[bgfx::RendererType::Count];
		const uint8_t nSupportedBackends = bgfx::getSupportedRenderers(supportedBackends);

		#define FORMAT "%-10s%s\n"
		std::string description;
		description += util::VarArgs(FORMAT, "<empty>", "Autodetect");

		for (const BackendMap &map : backendMaps)
		{
			uint8_t j;

			for (j = 0; j < nSupportedBackends; j++)
			{
				if (map.type == supportedBackends[j])
					break;
			}

			if (j != nSupportedBackends)
				description += util::VarArgs(FORMAT, map.id, bgfx::getRendererName(map.type));
		}

		ri.Cvar_SetDescription(backend, description.c_str());
		#undef FORMAT
	}

	bgfx_stats = ri.Cvar_Get("r_bgfx_stats", "0", CVAR_CHEAT);
	debugDraw = ri.Cvar_Get("r_debugDraw", "", 0);
	ri.Cvar_SetDescription(debugDraw,
		"<empty>    None\n"
		"depth      Linear depth\n"
		"dlight     Dynamic light data\n"
		"lum        Average and adapted luminance\n"
		"reflection Planar reflection\n"
		"smaa       SMAA edges and weights\n");
	debugDrawSize = ri.Cvar_Get("r_debugDrawSize", "256", CVAR_ARCHIVE);
	debugText = ri.Cvar_Get("r_debugText", "0", CVAR_CHEAT);
	dynamicLightIntensity = ri.Cvar_Get("r_dynamicLightIntensity", "1", CVAR_ARCHIVE);
	dynamicLightScale = ri.Cvar_Get("r_dynamicLightScale", "0.7", CVAR_ARCHIVE);
	hdr = ri.Cvar_Get("r_hdr", "0", CVAR_ARCHIVE | CVAR_LATCH);
	hdrKey = ri.Cvar_Get("r_hdrKey", "0.1", CVAR_ARCHIVE);
	lerpTextureAnimation = ri.Cvar_Get("r_lerpTextureAnimation", "1", CVAR_ARCHIVE | CVAR_LATCH);
	maxAnisotropy = ri.Cvar_Get("r_maxAnisotropy", "0", CVAR_ARCHIVE | CVAR_LATCH);
	picmip = ri.Cvar_Get("r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(picmip, 0, 16, qtrue);
	railWidth = ri.Cvar_Get("r_railWidth", "16", CVAR_ARCHIVE);
	railCoreWidth = ri.Cvar_Get("r_railCoreWidth", "6", CVAR_ARCHIVE);
	railSegmentLength = ri.Cvar_Get("r_railSegmentLength", "32", CVAR_ARCHIVE);
	softSprites = ri.Cvar_Get("r_softSprites", "1", CVAR_ARCHIVE);
	screenshotJpegQuality = ri.Cvar_Get("r_screenshotJpegQuality", "90", CVAR_ARCHIVE);
	waterReflections = ri.Cvar_Get("r_waterReflections", "0", CVAR_ARCHIVE | CVAR_LATCH);
	wireframe = ri.Cvar_Get("r_wireframe", "0", CVAR_CHEAT);

	// Screen
	brightness = ri.Cvar_Get("r_brightness", "1", CVAR_ARCHIVE);
	contrast = ri.Cvar_Get("r_contrast", "1", CVAR_ARCHIVE);
	gamma = ri.Cvar_Get("r_gamma", "1", CVAR_ARCHIVE);
	saturation = ri.Cvar_Get("r_saturation", "1", CVAR_ARCHIVE);

	// Window
	allowResize = ri.Cvar_Get("r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH);
	centerWindow = ri.Cvar_Get("r_centerWindow", "1", CVAR_ARCHIVE | CVAR_LATCH);
	customheight = ri.Cvar_Get("r_customheight", "1080", CVAR_ARCHIVE | CVAR_LATCH);
	customwidth = ri.Cvar_Get("r_customwidth", "1920", CVAR_ARCHIVE | CVAR_LATCH);
	customPixelAspect = ri.Cvar_Get("r_customPixelAspect", "1", CVAR_ARCHIVE | CVAR_LATCH);
	fullscreen = ri.Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE);
	mode = ri.Cvar_Get("r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH);
	noborder = ri.Cvar_Get("r_noborder", "0", CVAR_ARCHIVE | CVAR_LATCH);
}

static void TakeScreenshot(const char *extension)
{
	const bool silent = !strcmp(ri.Cmd_Argv(1), "silent");
	static int lastNumber = -1;
	char filename[MAX_OSPATH];

	if (ri.Cmd_Argc() == 2 && !silent)
	{
		// Explicit filename.
		util::Sprintf(filename, MAX_OSPATH, "screenshots/%s.%s", ri.Cmd_Argv(1), extension);
	}
	else
	{
		// Scan for a free filename.
		// If we have saved a previous screenshot, don't scan again, because recording demo avis can involve thousands of shots.
		if (lastNumber == -1)
			lastNumber = 0;

		// Scan for a free number.
		for (; lastNumber <= 9999; lastNumber++)
		{
			if (lastNumber < 0 || lastNumber > 9999)
			{
				util::Sprintf(filename, MAX_OSPATH, "screenshots/shot9999.%s", extension);
			}
			else
			{
				int a = lastNumber / 1000;
				lastNumber -= a * 1000;
				int b = lastNumber / 100;
				lastNumber -= b * 100;
				int c = lastNumber / 10;
				lastNumber -= c * 10;
				int d = lastNumber;
				util::Sprintf(filename, MAX_OSPATH, "screenshots/shot%i%i%i%i.%s", a, b, c, d, extension);
			}

			if (!ri.FS_FileExists(filename))
				break; // File doesn't exist.
		}

		if (lastNumber >= 9999)
		{
			ri.Printf(PRINT_ALL, "ScreenShot: Couldn't create a file\n"); 
			return;
		}

		lastNumber++;
	}

	bgfx::saveScreenShot(filename);

	if (!silent)
		ri.Printf(PRINT_ALL, "Wrote %s\n", filename);
}

static void Cmd_PrintMaterials()
{
	if (g_materialCache)
		g_materialCache->printMaterials();
}

static void Cmd_Screenshot()
{
	TakeScreenshot("tga");
}

static void Cmd_ScreenshotJPEG()
{
	TakeScreenshot("jpg");
}

static void Cmd_ScreenshotPNG()
{
	TakeScreenshot("png");
}

const FrameBuffer Main::defaultFb_;

AntiAliasing AntiAliasingFromString(const char *s)
{
	if (util::Stricmp(s, "fxaa") == 0)
		return AntiAliasing::FXAA;
	else if (util::Stricmp(s, "msaa2x") == 0)
		return AntiAliasing::MSAA2x;
	else if (util::Stricmp(s, "msaa4x") == 0)
		return AntiAliasing::MSAA4x;
	else if (util::Stricmp(s, "msaa8x") == 0)
		return AntiAliasing::MSAA8x;
	else if (util::Stricmp(s, "msaa16x") == 0)
		return AntiAliasing::MSAA16x;
	else if (util::Stricmp(s, "smaa") == 0)
		return AntiAliasing::SMAA;

	return AntiAliasing::None;
}

Main::Main()
{
	g_cvars.initialize();
	aa_ = AntiAliasingFromString(g_cvars.aa->string);

	// Don't allow MSAA if HDR is enabled.
	if (g_cvars.hdr->integer && aa_ >= AntiAliasing::MSAA2x && aa_ <= AntiAliasing::MSAA16x)
		aa_ = AntiAliasing::None;

	aaHud_ = AntiAliasingFromString(g_cvars.aa_hud->string);

	// Non-world/HUD scenes can only use MSAA.
	if (!(aaHud_ >= AntiAliasing::MSAA2x && aaHud_ <= AntiAliasing::MSAA16x))
		aaHud_ = AntiAliasing::None;

	softSpritesEnabled_ = g_cvars.softSprites->integer && !(aa_ >= AntiAliasing::MSAA2x && aa_ <= AntiAliasing::MSAA16x);

	ri.Cmd_AddCommand("r_printMaterials", Cmd_PrintMaterials);
	ri.Cmd_AddCommand("screenshot", Cmd_Screenshot);
	ri.Cmd_AddCommand("screenshotJPEG", Cmd_ScreenshotJPEG);
	ri.Cmd_AddCommand("screenshotPNG", Cmd_ScreenshotPNG);

	// init function tables
	for (size_t i = 0; i < g_funcTableSize; i++)
	{
		g_sinTable[i] = sin(DEG2RAD(i * 360.0f / float(g_funcTableSize - 1)));
		g_squareTable[i] = (i < g_funcTableSize / 2) ? 1.0f : -1.0f;
		g_sawToothTable[i] = (float)i / g_funcTableSize;
		g_inverseSawToothTable[i] = 1.0f - g_sawToothTable[i];

		if (i < g_funcTableSize / 2)
		{
			if (i < g_funcTableSize / 4)
			{
				g_triangleTable[i] = (float) i / (g_funcTableSize / 4);
			}
			else
			{
				g_triangleTable[i] = 1.0f - g_triangleTable[i - g_funcTableSize / 4];
			}
		}
		else
		{
			g_triangleTable[i] = -g_triangleTable[i - g_funcTableSize / 2];
		}
	}

	for (int i = 0; i < noiseSize_; i++)
	{
		noiseTable_[i] = (float)(((rand() / (float)RAND_MAX) * 2.0 - 1.0));
		noisePerm_[i] = (unsigned char)(rand() / (float)RAND_MAX * 255);
	}

	meta::Initialize();
}

Main::~Main()
{
	if (aa_ == AntiAliasing::SMAA)
	{
		if (bgfx::isValid(smaaAreaTex_))
			bgfx::destroyTexture(smaaAreaTex_);
		if (bgfx::isValid(smaaSearchTex_))
			bgfx::destroyTexture(smaaSearchTex_);
	}

	ri.Cmd_RemoveCommand("r_printMaterials");
	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("screenshotJPEG");
	ri.Cmd_RemoveCommand("screenshotPNG");
	g_materialCache = nullptr;
	g_modelCache = nullptr;
	Texture::shutdownCache();
}

struct ShaderProgramIdMap
{
	FragmentShaderId::Enum frag;
	VertexShaderId::Enum vert;
};

void Main::initialize()
{
	// Create a window if we don't have one.
	if (window::GetWidth() == 0)
	{
		// Get the selected backend, and make sure it's actually supported.
		bgfx::RendererType::Enum supportedBackends[bgfx::RendererType::Count];
		const uint8_t nSupportedBackends = bgfx::getSupportedRenderers(supportedBackends);
		bgfx::RendererType::Enum selectedBackend = bgfx::RendererType::Count;

		for (const BackendMap &map : backendMaps)
		{
			uint8_t j;

			for (j = 0; j < nSupportedBackends; j++)
			{
				if (map.type == supportedBackends[j])
					break;
			}

			if (j == nSupportedBackends)
				continue; // Not supported.

			if (!util::Stricmp(g_cvars.backend->string, map.id))
			{
				selectedBackend = map.type;
				break;
			}
		}

		window::Initialize();
		
		if (!bgfx::init(selectedBackend, 0, 0, &bgfxCallback))
		{
			ri.Error(ERR_DROP, "bgfx init failed");
		}

		// Print the chosen backend name. It may not be the one that was selected.
		const bool forced = selectedBackend != bgfx::RendererType::Count && selectedBackend != bgfx::getCaps()->rendererType;
		ri.Printf(PRINT_ALL, "Renderer backend%s: %s\n", forced ? " forced to" : "", bgfx::getRendererName(bgfx::getCaps()->rendererType));
	}

	uint32_t resetFlags = 0;

	if (aaHud_ >= AntiAliasing::MSAA2x && aaHud_ <= AntiAliasing::MSAA16x)
	{
		resetFlags |= (1 + (int)aaHud_ - (int)AntiAliasing::MSAA2x) << BGFX_RESET_MSAA_SHIFT;
	}

	if (g_cvars.maxAnisotropy->integer)
	{
		resetFlags |= BGFX_RESET_MAXANISOTROPY;
	}

	bgfx::reset(window::GetWidth(), window::GetHeight(), resetFlags);
	const bgfx::Caps *caps = bgfx::getCaps();

	if (caps->maxFBAttachments < 2)
	{
		ri.Error(ERR_DROP, "MRT not supported");
	}

	if ((caps->formats[bgfx::TextureFormat::R8U] & BGFX_CAPS_FORMAT_TEXTURE_2D) == 0)
	{
		ri.Error(ERR_DROP, "R8U texture format not supported");
	}

	if ((caps->formats[bgfx::TextureFormat::R16U] & BGFX_CAPS_FORMAT_TEXTURE_2D) == 0)
	{
		ri.Error(ERR_DROP, "R16U texture format not supported");
	}

	debugDraw_ = DebugDrawFromString(g_cvars.debugDraw->string);
	halfTexelOffset_ = caps->rendererType == bgfx::RendererType::Direct3D9 ? 0.5f : 0;
	isTextureOriginBottomLeft_ = caps->rendererType == bgfx::RendererType::OpenGL || caps->rendererType == bgfx::RendererType::OpenGLES;
	Vertex::init();
	uniforms_ = std::make_unique<Uniforms>();
	entityUniforms_ = std::make_unique<Uniforms_Entity>();
	matUniforms_ = std::make_unique<Uniforms_Material>();
	matStageUniforms_ = std::make_unique<Uniforms_MaterialStage>();
	Texture::initializeCache();
	materialCache_ = std::make_unique<MaterialCache>();
	g_materialCache = materialCache_.get();
	modelCache_ = std::make_unique<ModelCache>();
	g_modelCache = modelCache_.get();
	dlightManager_ = std::make_unique<DynamicLightManager>();

	// Get shader ID to shader source string mappings.
	std::array<ShaderSourceMem, FragmentShaderId::Num> fragMem;
	std::array<ShaderSourceMem, VertexShaderId::Num> vertMem;

	if (caps->rendererType == bgfx::RendererType::OpenGL)
	{
		fragMem = GetFragmentShaderSourceMap_gl();
		vertMem = GetVertexShaderSourceMap_gl();
	}
#ifdef WIN32
	else if (caps->rendererType == bgfx::RendererType::Direct3D11)
	{
		fragMem = GetFragmentShaderSourceMap_d3d11();
		vertMem = GetVertexShaderSourceMap_d3d11();
	}
#endif

	// Map shader programs to their vertex and fragment shaders.
	std::array<ShaderProgramIdMap, ShaderProgramId::Num> programMap;
	programMap[ShaderProgramId::AdaptedLuminance] = { FragmentShaderId::AdaptedLuminance, VertexShaderId::Texture };
	programMap[ShaderProgramId::Color]            = { FragmentShaderId::Color, VertexShaderId::Color };
	programMap[ShaderProgramId::Depth]            = { FragmentShaderId::Depth, VertexShaderId::Depth };

	programMap[ShaderProgramId::Depth + DepthShaderProgramVariant::AlphaTest] =
	{
		FragmentShaderId::Depth_AlphaTest,
		VertexShaderId::Depth_AlphaTest
	};

	programMap[ShaderProgramId::Depth + DepthShaderProgramVariant::DepthRange] =
	{
		FragmentShaderId::Depth,
		VertexShaderId::Depth_DepthRange
	};

	programMap[ShaderProgramId::Depth + (DepthShaderProgramVariant::AlphaTest | DepthShaderProgramVariant::DepthRange)] =
	{
		FragmentShaderId::Depth_AlphaTest,
		VertexShaderId::Depth_AlphaTestDepthRange
	};

	programMap[ShaderProgramId::Fog] = { FragmentShaderId::Fog, VertexShaderId::Fog };

	programMap[ShaderProgramId::Fog + FogShaderProgramVariant::DepthRange] =
	{
		FragmentShaderId::Fog,
		VertexShaderId::Fog_DepthRange
	};

	programMap[ShaderProgramId::FXAA] = { FragmentShaderId::FXAA, VertexShaderId::Texture };

	// Sync with GenericShaderProgramVariant. Order matters - fragment first.
	for (int y = 0; y < GenericVertexShaderVariant::Num; y++)
	{
		for (int x = 0; x < GenericFragmentShaderVariant::Num; x++)
		{
			ShaderProgramIdMap &pm = programMap[ShaderProgramId::Generic + x + y * GenericFragmentShaderVariant::Num];
			pm.frag = FragmentShaderId::Enum(FragmentShaderId::Generic + x);
			pm.vert = VertexShaderId::Enum(VertexShaderId::Generic + y);
		}
	}

	programMap[ShaderProgramId::LinearDepth]          = { FragmentShaderId::LinearDepth, VertexShaderId::Texture };
	programMap[ShaderProgramId::Luminance]            = { FragmentShaderId::Luminance, VertexShaderId::Texture };
	programMap[ShaderProgramId::LuminanceDownsample]  = { FragmentShaderId::LuminanceDownsample, VertexShaderId::Texture };
	programMap[ShaderProgramId::SMAABlendingWeightCalculation] = { FragmentShaderId::SMAABlendingWeightCalculation, VertexShaderId::SMAABlendingWeightCalculation };
	programMap[ShaderProgramId::SMAAEdgeDetection]    = { FragmentShaderId::SMAAEdgeDetection, VertexShaderId::SMAAEdgeDetection };
	programMap[ShaderProgramId::SMAANeighborhoodBlending] = { FragmentShaderId::SMAANeighborhoodBlending, VertexShaderId::SMAANeighborhoodBlending };
	programMap[ShaderProgramId::Texture]              = { FragmentShaderId::Texture, VertexShaderId::Texture };
	programMap[ShaderProgramId::TextureColor]         = { FragmentShaderId::TextureColor, VertexShaderId::Texture };
	programMap[ShaderProgramId::TextureSingleChannel] = { FragmentShaderId::TextureSingleChannel, VertexShaderId::Texture };
	programMap[ShaderProgramId::ToneMap]              = { FragmentShaderId::ToneMap, VertexShaderId::Texture };

	// Create shader programs.
	for (size_t i = 0; i < ShaderProgramId::Num; i++)
	{
		// Don't create shader programs that won't be used.
		if (aa_ != AntiAliasing::FXAA && i == ShaderProgramId::FXAA)
			continue;

		if (aa_ != AntiAliasing::SMAA && (i == ShaderProgramId::SMAABlendingWeightCalculation || i == ShaderProgramId::SMAAEdgeDetection || i == ShaderProgramId::SMAANeighborhoodBlending))
			continue;

		if (g_cvars.hdr->integer == 0 && (i == ShaderProgramId::AdaptedLuminance || i == ShaderProgramId::Luminance || i == ShaderProgramId::LuminanceDownsample || i == ShaderProgramId::ToneMap))
			continue;

		Shader &fragment = fragmentShaders_[programMap[i].frag];

		if (!bgfx::isValid(fragment.handle))
		{
			fragment.handle = bgfx::createShader(bgfx::makeRef(fragMem[programMap[i].frag].mem, (uint32_t)fragMem[programMap[i].frag].size));

			if (!bgfx::isValid(fragment.handle))
				ri.Error(ERR_DROP, "Error creating fragment shader");
		}

		Shader &vertex = vertexShaders_[programMap[i].vert];
	
		if (!bgfx::isValid(vertex.handle))
		{
			vertex.handle = bgfx::createShader(bgfx::makeRef(vertMem[programMap[i].vert].mem, (uint32_t)vertMem[programMap[i].vert].size));

			if (!bgfx::isValid(vertex.handle))
				ri.Error(ERR_DROP, "Error creating vertex shader");
		}

		shaderPrograms_[i].handle = bgfx::createProgram(vertex.handle, fragment.handle);

		if (!bgfx::isValid(shaderPrograms_[i].handle))
			ri.Error(ERR_DROP, "Error creating shader program");
	}
}

namespace main {

void AddDynamicLightToScene(const DynamicLight &light)
{
	s_main->addDynamicLightToScene(light);
}

void AddEntityToScene(const Entity &entity)
{
	return s_main->addEntityToScene(entity);
}

void AddPolyToScene(qhandle_t hShader, int nVerts, const polyVert_t *verts, int nPolys)
{
	return s_main->addPolyToScene(hShader, nVerts, verts, nPolys);
}

void DebugPrint(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	char text[1024];
	util::Vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	s_main->debugPrint(text);
}

void DrawBounds(const Bounds &bounds)
{
	s_main->drawBounds(bounds);
}

void DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int materialIndex)
{
	s_main->drawStretchPic(x, y, w, h, s1, t1, s2, t2, materialIndex);
}

void DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty)
{
	s_main->drawStretchRaw(x, y, w, h, cols, rows, data, client, dirty);
}

void EndFrame()
{
	s_main->endFrame();
}

const Entity *GetCurrentEntity()
{
	return s_main->getCurrentEntity();
}

float GetFloatTime()
{
	return s_main->getFloatTime();
}

float GetNoise(float x, float y, float z, float t)
{
	return s_main->getNoise(x, y, z, t);
}

void Initialize()
{
	s_main = std::make_unique<Main>();
	s_main->initialize();
}

bool isCameraMirrored()
{
	return s_main->isCameraMirrored();
}

void LoadWorld(const char *name)
{
	s_main->loadWorld(name);
}

void RegisterFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	s_main->registerFont(fontName, pointSize, font);
}

void RenderScene(const SceneDefinition &scene)
{
	s_main->renderScene(scene);
}

bool SampleLight(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir)
{
	return s_main->sampleLight(position, ambientLight, directedLight, lightDir);
}

void SetColor(vec4 c)
{
	s_main->setColor(c);
}

void SetSunLight(const SunLight &sunLight)
{
	s_main->setSunLight(sunLight);
}

void Shutdown()
{
	s_main.reset(nullptr);
}

void UploadCinematic(int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty)
{
	s_main->uploadCinematic(w, h, cols, rows, data, client, dirty);
}

} // namespace main
} // namespace renderer
