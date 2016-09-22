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
#include "../smaa/AreaTex.h"
#include "../smaa/SearchTex.h"

struct BackendMap
{
	bgfx::RendererType::Enum type;
	const char *id; // Used by r_backend cvar.
};

static const std::array<const BackendMap, 5> s_backendMaps =
{{
	{ bgfx::RendererType::Null, "null" },
	{ bgfx::RendererType::Direct3D11, "d3d11" },
	{ bgfx::RendererType::Direct3D12, "d3d12" },
	{ bgfx::RendererType::OpenGL, "gl" },
	{ bgfx::RendererType::Vulkan, "vulkan" }
}};

void ConsoleVariables::initialize()
{
	aa = interface::Cvar_Get("r_aa", "", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	aa.setDescription(
		"<empty>   None\n"
		"msaa2x    MSAA 2x\n"
		"msaa4x    MSAA 4x\n"
		"msaa8x    MSAA 8x\n"
		"msaa16x   MSAA 16x\n"
		"smaa      SMAA 1x\n");
	aa_hud = interface::Cvar_Get("r_aa_hud", "", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	aa_hud.setDescription(
		"<empty>   None\n"
		"msaa2x    MSAA 2x\n"
		"msaa4x    MSAA 4x\n"
		"msaa8x    MSAA 8x\n"
		"msaa16x   MSAA 16x\n");
	backend = interface::Cvar_Get("r_backend", "", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);

	{
		// Have the r_backend cvar print a list of supported backends when invoked without any arguments.
		bgfx::RendererType::Enum supportedBackends[bgfx::RendererType::Count];
		const uint8_t nSupportedBackends = bgfx::getSupportedRenderers(supportedBackends);

		#define FORMAT "%-10s%s\n"
		std::string description;
		description += util::VarArgs(FORMAT, "<empty>", "Autodetect");

		for (const BackendMap &map : s_backendMaps)
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

		backend.setDescription(description.c_str());
		#undef FORMAT
	}

	bgfx_stats = interface::Cvar_Get("r_bgfx_stats", "0", ConsoleVariableFlags::Cheat);
	bloom = interface::Cvar_Get("r_bloom", "1", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	bloomScale = interface::Cvar_Get("r_bloomScale", "1.0", ConsoleVariableFlags::Archive);
	debug = interface::Cvar_Get("r_debug", "", 0);
	debugDraw = interface::Cvar_Get("r_debugDraw", "", 0);
	debugDraw.setDescription(
		"<empty>    None\n"
		"bloom      Bloom\n"
		"depth      Linear depth\n"
		"dlight     Dynamic light data\n"
		"lightmap   Lightmaps\n"
		"reflection Planar reflection\n"
		"smaa       SMAA edges and weights\n");
	debugDrawSize = interface::Cvar_Get("r_debugDrawSize", "256", ConsoleVariableFlags::Archive);
	dynamicLightIntensity = interface::Cvar_Get("r_dynamicLightIntensity", "1", ConsoleVariableFlags::Archive);
	dynamicLightScale = interface::Cvar_Get("r_dynamicLightScale", "0.7", ConsoleVariableFlags::Archive);
	lerpTextureAnimation = interface::Cvar_Get("r_lerpTextureAnimation", "1", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	maxAnisotropy = interface::Cvar_Get("r_maxAnisotropy", "0", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	picmip = interface::Cvar_Get("r_picmip", "0", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	picmip.checkRange(0, 16, true);
	railWidth = interface::Cvar_Get("r_railWidth", "16", ConsoleVariableFlags::Archive);
	railCoreWidth = interface::Cvar_Get("r_railCoreWidth", "6", ConsoleVariableFlags::Archive);
	railSegmentLength = interface::Cvar_Get("r_railSegmentLength", "32", ConsoleVariableFlags::Archive);
	softSprites = interface::Cvar_Get("r_softSprites", "1", ConsoleVariableFlags::Archive);
	screenshotJpegQuality = interface::Cvar_Get("r_screenshotJpegQuality", "90", ConsoleVariableFlags::Archive);
	waterReflections = interface::Cvar_Get("r_waterReflections", "0", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	wireframe = interface::Cvar_Get("r_wireframe", "0", ConsoleVariableFlags::Cheat);

	// Gamma
	gamma = interface::Cvar_Get("r_gamma", "1", ConsoleVariableFlags::Archive);
	ignoreHardwareGamma = interface::Cvar_Get("r_ignorehwgamma", "0", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);

	// Window
	allowResize = interface::Cvar_Get("r_allowResize", "0", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	centerWindow = interface::Cvar_Get("r_centerWindow", "1", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	customheight = interface::Cvar_Get("r_customheight", "1080", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	customwidth = interface::Cvar_Get("r_customwidth", "1920", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	customPixelAspect = interface::Cvar_Get("r_customPixelAspect", "1", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	fullscreen = interface::Cvar_Get("r_fullscreen", "1", ConsoleVariableFlags::Archive);
	mode = interface::Cvar_Get("r_mode", "-2", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
	noborder = interface::Cvar_Get("r_noborder", "0", ConsoleVariableFlags::Archive | ConsoleVariableFlags::Latch);
}

namespace main {

static BgfxCallback bgfxCallback;

static AntiAliasing AntiAliasingFromString(const char *s)
{
	if (util::Stricmp(s, "msaa2x") == 0)
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

static void TakeScreenshot(const char *extension)
{
	static int lastNumber = -1;
	char filename[MAX_OSPATH];

	// Normally the "Wrote %s" message would be printed in this function, but since BGFX screenshots the next frame, that message would show up in the screenshot.
	// Pass the silent arg by shoving it in the first character of the filename.
	const char silent = !strcmp(interface::Cmd_Argv(1), "silent") ? 'y' : 'n';

	if (interface::Cmd_Argc() == 2 && !silent)
	{
		// Explicit filename.
		util::Sprintf(filename, MAX_OSPATH, "%cscreenshots/%s.%s", silent, interface::Cmd_Argv(1), extension);
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
				util::Sprintf(filename, MAX_OSPATH, "%cscreenshots/shot9999.%s", silent, extension);
			}
			else
			{
				int number = lastNumber;
				int a = number / 1000;
				number -= a * 1000;
				int b = number / 100;
				number -= b * 100;
				int c = number / 10;
				number -= c * 10;
				int d = number;
				util::Sprintf(filename, MAX_OSPATH, "%cscreenshots/shot%i%i%i%i.%s", silent, a, b, c, d, extension);
			}

			if (!interface::FS_FileExists(filename + 1)) // Skip first char - silent arg.
				break; // File doesn't exist.
		}

		if (lastNumber >= 9999)
		{
			interface::Printf("ScreenShot: Couldn't create a file\n"); 
			return;
		}

		lastNumber++;
	}

	bgfx::saveScreenShot(filename);
}

static void Cmd_BakeLights()
{
	int nSamples = 1;

	if (interface::Cmd_Argc() > 1)
	{
		nSamples = atoi(interface::Cmd_Argv(1));
	}

	light_baker::Start(nSamples);
}

static void Cmd_PickMaterial()
{
	if (world::IsLoaded())
	{
		world::PickMaterial();
	}
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

struct ShaderProgramIdMap
{
	FragmentShaderId::Enum frag;
	VertexShaderId::Enum vert;
};

void Initialize()
{
	s_main = std::make_unique<Main>();
	g_cvars.initialize();
	s_main->aa = AntiAliasingFromString(g_cvars.aa.getString());
	s_main->aaHud = AntiAliasingFromString(g_cvars.aa_hud.getString());

	// Non-world/HUD scenes can only use MSAA.
	if (!(s_main->aaHud >= AntiAliasing::MSAA2x && s_main->aaHud <= AntiAliasing::MSAA16x))
		s_main->aaHud = AntiAliasing::None;

	s_main->softSpritesEnabled = g_cvars.softSprites.getBool() && !(s_main->aa >= AntiAliasing::MSAA2x && s_main->aa <= AntiAliasing::MSAA16x);

	interface::Cmd_Add("r_bakeLights", Cmd_BakeLights);
	interface::Cmd_Add("r_pickMaterial", Cmd_PickMaterial);
	interface::Cmd_Add("r_printMaterials", Cmd_PrintMaterials);
	interface::Cmd_Add("screenshot", Cmd_Screenshot);
	interface::Cmd_Add("screenshotJPEG", Cmd_ScreenshotJPEG);
	interface::Cmd_Add("screenshotPNG", Cmd_ScreenshotPNG);

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

	for (int i = 0; i < s_main->noiseSize; i++)
	{
		s_main->noiseTable[i] = (float)(((rand() / (float)RAND_MAX) * 2.0 - 1.0));
		s_main->noisePerm[i] = (unsigned char)(rand() / (float)RAND_MAX * 255);
	}

	meta::Initialize();

	// Create a window if we don't have one.
	if (window::GetWidth() == 0)
	{
		window::Initialize();
		g_hardwareGammaEnabled = window::IsFullscreen() && !g_cvars.ignoreHardwareGamma.getBool();
		SetWindowGamma();

		// Get the selected backend, and make sure it's actually supported.
		bgfx::RendererType::Enum supportedBackends[bgfx::RendererType::Count];
		const uint8_t nSupportedBackends = bgfx::getSupportedRenderers(supportedBackends);
		bgfx::RendererType::Enum selectedBackend = bgfx::RendererType::Count;

		for (const BackendMap &map : s_backendMaps)
		{
			uint8_t j;

			for (j = 0; j < nSupportedBackends; j++)
			{
				if (map.type == supportedBackends[j])
					break;
			}

			if (j == nSupportedBackends)
				continue; // Not supported.

			if (!util::Stricmp(g_cvars.backend.getString(), map.id))
			{
				selectedBackend = map.type;
				break;
			}
		}
		
		if (!bgfx::init(selectedBackend, 0, 0, &bgfxCallback))
		{
			interface::Error("bgfx init failed");
		}

		// Print the chosen backend name. It may not be the one that was selected.
		const bool forced = selectedBackend != bgfx::RendererType::Count && selectedBackend != bgfx::getCaps()->rendererType;
		interface::Printf("Renderer backend%s: %s\n", forced ? " forced to" : "", bgfx::getRendererName(bgfx::getCaps()->rendererType));
		interface::Printf("   texture blit %ssupported\n", (bgfx::getCaps()->supported & BGFX_CAPS_TEXTURE_BLIT) == 0 ? "NOT " : "");
		interface::Printf("   texture read back %ssupported\n", (bgfx::getCaps()->supported & BGFX_CAPS_TEXTURE_READ_BACK) == 0 ? "NOT " : "");
	}

	uint32_t resetFlags = 0;

	if (s_main->aaHud >= AntiAliasing::MSAA2x && s_main->aaHud <= AntiAliasing::MSAA16x)
	{
		resetFlags |= (1 + (int)s_main->aaHud - (int)AntiAliasing::MSAA2x) << BGFX_RESET_MSAA_SHIFT;
	}

	if (g_cvars.maxAnisotropy.getBool())
	{
		resetFlags |= BGFX_RESET_MAXANISOTROPY;
	}

	bgfx::reset(window::GetWidth(), window::GetHeight(), resetFlags);
	const bgfx::Caps *caps = bgfx::getCaps();

	if (caps->maxFBAttachments < 2)
	{
		interface::Error("MRT not supported");
	}

	if ((caps->formats[bgfx::TextureFormat::R8U] & BGFX_CAPS_FORMAT_TEXTURE_2D) == 0)
	{
		interface::Error("R8U texture format not supported");
	}

	if ((caps->formats[bgfx::TextureFormat::R16U] & BGFX_CAPS_FORMAT_TEXTURE_2D) == 0)
	{
		interface::Error("R16U texture format not supported");
	}

	s_main->debugDraw = DebugDrawFromString(g_cvars.debugDraw.getString());
	s_main->halfTexelOffset = caps->rendererType == bgfx::RendererType::Direct3D9 ? 0.5f : 0;
	s_main->isTextureOriginBottomLeft = caps->rendererType == bgfx::RendererType::OpenGL || caps->rendererType == bgfx::RendererType::OpenGLES;
	Vertex::init();
	s_main->uniforms = std::make_unique<Uniforms>();
	s_main->entityUniforms = std::make_unique<Uniforms_Entity>();
	s_main->matUniforms = std::make_unique<Uniforms_Material>();
	s_main->matStageUniforms = std::make_unique<Uniforms_MaterialStage>();
	Texture::initializeCache();
	s_main->materialCache = std::make_unique<MaterialCache>();
	g_materialCache = s_main->materialCache.get();
	s_main->modelCache = std::make_unique<ModelCache>();
	g_modelCache = s_main->modelCache.get();
	s_main->dlightManager = std::make_unique<DynamicLightManager>();

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
	programMap[ShaderProgramId::Bloom] = { FragmentShaderId::Bloom, VertexShaderId::Texture };
	programMap[ShaderProgramId::Color] = { FragmentShaderId::Color, VertexShaderId::Color };
	programMap[ShaderProgramId::Depth] = { FragmentShaderId::Depth, VertexShaderId::Depth };

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

	programMap[ShaderProgramId::Fog + FogShaderProgramVariant::Bloom] =
	{
		FragmentShaderId::Fog_Bloom,
		VertexShaderId::Fog
	};

	programMap[ShaderProgramId::Fog + FogShaderProgramVariant::DepthRange] =
	{
		FragmentShaderId::Fog,
		VertexShaderId::Fog_DepthRange
	};

	programMap[ShaderProgramId::Fog + (FogShaderProgramVariant::Bloom | FogShaderProgramVariant::DepthRange)] =
	{
		FragmentShaderId::Fog_Bloom,
		VertexShaderId::Fog_DepthRange
	};

	programMap[ShaderProgramId::GaussianBlur] = { FragmentShaderId::GaussianBlur, VertexShaderId::Texture };

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

	programMap[ShaderProgramId::LinearDepth] = { FragmentShaderId::LinearDepth, VertexShaderId::Texture };
	programMap[ShaderProgramId::HemicubeDownsample] = { FragmentShaderId::HemicubeDownsample, VertexShaderId::Texture };
	programMap[ShaderProgramId::HemicubeWeightedDownsample] = { FragmentShaderId::HemicubeWeightedDownsample, VertexShaderId::Texture };
	programMap[ShaderProgramId::SMAABlendingWeightCalculation] = { FragmentShaderId::SMAABlendingWeightCalculation, VertexShaderId::SMAABlendingWeightCalculation };
	programMap[ShaderProgramId::SMAAEdgeDetection] = { FragmentShaderId::SMAAEdgeDetection, VertexShaderId::SMAAEdgeDetection };
	programMap[ShaderProgramId::SMAANeighborhoodBlending] = { FragmentShaderId::SMAANeighborhoodBlending, VertexShaderId::SMAANeighborhoodBlending };
	programMap[ShaderProgramId::Texture] = { FragmentShaderId::Texture, VertexShaderId::Texture };
	programMap[ShaderProgramId::TextureColor] = { FragmentShaderId::TextureColor, VertexShaderId::Texture };
	programMap[ShaderProgramId::TextureDebug] = { FragmentShaderId::TextureDebug, VertexShaderId::Texture };

	// Create shader programs.
	for (size_t i = 0; i < ShaderProgramId::Num; i++)
	{
		// Don't create shader programs that won't be used.
		if (s_main->aa != AntiAliasing::SMAA && (i == ShaderProgramId::SMAABlendingWeightCalculation || i == ShaderProgramId::SMAAEdgeDetection || i == ShaderProgramId::SMAANeighborhoodBlending))
			continue;

		if (g_cvars.bloom.getBool() == 0 && (i == ShaderProgramId::Bloom || i == ShaderProgramId::GaussianBlur))
			continue;

		Shader &fragment = s_main->fragmentShaders[programMap[i].frag];

		if (!bgfx::isValid(fragment.handle))
		{
			fragment.handle = bgfx::createShader(bgfx::makeRef(fragMem[programMap[i].frag].mem, (uint32_t)fragMem[programMap[i].frag].size));

			if (!bgfx::isValid(fragment.handle))
				interface::Error("Error creating fragment shader");
		}

		Shader &vertex = s_main->vertexShaders[programMap[i].vert];
	
		if (!bgfx::isValid(vertex.handle))
		{
			vertex.handle = bgfx::createShader(bgfx::makeRef(vertMem[programMap[i].vert].mem, (uint32_t)vertMem[programMap[i].vert].size));

			if (!bgfx::isValid(vertex.handle))
				interface::Error("Error creating vertex shader");
		}

		s_main->shaderPrograms[i].handle = bgfx::createProgram(vertex.handle, fragment.handle);

		if (!bgfx::isValid(s_main->shaderPrograms[i].handle))
			interface::Error("Error creating shader program");
	}
}

void LoadWorld(const char *name)
{
	if (world::IsLoaded())
	{
		interface::Error("ERROR: attempted to redundantly load world map");
	}

	// Create frame buffers first.
	uint32_t aaFlags = 0;

	if (s_main->aa >= AntiAliasing::MSAA2x && s_main->aa <= AntiAliasing::MSAA16x)
	{
		aaFlags |= (1 + (int)s_main->aa - (int)AntiAliasing::MSAA2x) << BGFX_TEXTURE_RT_MSAA_SHIFT;
	}

	const uint32_t rtClampFlags = BGFX_TEXTURE_RT | BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP;
	s_main->linearDepthFb.handle = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Equal, bgfx::TextureFormat::R16F);

	if (g_cvars.bloom.getBool())
	{
		if (s_main->aa == AntiAliasing::SMAA)
		{
			// Bloom needs a temp BGRA8 destination for SMAA.
			s_main->sceneTempFb.handle = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Equal, bgfx::TextureFormat::BGRA8, rtClampFlags);
		}		
		
		bgfx::TextureHandle sceneTextures[3];
		sceneTextures[0] = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::BGRA8, rtClampFlags | aaFlags);
		sceneTextures[1] = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::BGRA8, rtClampFlags | aaFlags);
		sceneTextures[2] = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | aaFlags);
		s_main->sceneFb.handle = bgfx::createFrameBuffer(3, sceneTextures, true);
		s_main->sceneBloomAttachment = 1;
		s_main->sceneDepthAttachment = 2;

		for (size_t i = 0; i < s_main->nBloomFrameBuffers; i++)
		{
			s_main->bloomFb[i].handle = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Quarter, bgfx::TextureFormat::BGRA8, rtClampFlags | aaFlags);
		}
	}
	else
	{
		bgfx::TextureHandle sceneTextures[2];
		sceneTextures[0] = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::BGRA8, rtClampFlags | aaFlags);
		sceneTextures[1] = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | aaFlags);
		s_main->sceneFb.handle = bgfx::createFrameBuffer(2, sceneTextures, true);
		s_main->sceneDepthAttachment = 1;
	}

	if (g_cvars.waterReflections.getBool())
	{
		bgfx::TextureHandle reflectionTexture = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::BGRA8, rtClampFlags | aaFlags);
		s_main->reflectionFb.handle = bgfx::createFrameBuffer(1, &reflectionTexture); // Don't destroy the texture, that will be done by the texture cache.

		// Register the reflection texture so it can accessed by materials.
		Texture::create("*reflection", reflectionTexture);
	}

	if (s_main->aa == AntiAliasing::SMAA)
	{
		s_main->smaaBlendFb.handle = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Equal, bgfx::TextureFormat::BGRA8, rtClampFlags);
		s_main->smaaEdgesFb.handle = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Equal, bgfx::TextureFormat::RG8, rtClampFlags);
		s_main->smaaAreaTex = bgfx::createTexture2D(AREATEX_WIDTH, AREATEX_HEIGHT, false, 1, bgfx::TextureFormat::RG8, BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP, bgfx::makeRef(areaTexBytes, AREATEX_SIZE));
		s_main->smaaSearchTex = bgfx::createTexture2D(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, false, 1, bgfx::TextureFormat::R8, BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP, bgfx::makeRef(searchTexBytes, SEARCHTEX_SIZE));
	}

	// Load the world.
	world::Load(name);
	s_main->dlightManager->initializeGrid();
}

void Shutdown(bool destroyWindow)
{
	light_baker::Stop();
	light_baker::Shutdown();
	world::Unload();
	interface::Cmd_Remove("r_bakeLights");
	interface::Cmd_Remove("r_pickMaterial");
	interface::Cmd_Remove("r_printMaterials");
	interface::Cmd_Remove("screenshot");
	interface::Cmd_Remove("screenshotJPEG");
	interface::Cmd_Remove("screenshotPNG");
	g_materialCache = nullptr;
	g_modelCache = nullptr;
	Texture::shutdownCache();

	if (s_main.get())
	{
		if (s_main->aa == AntiAliasing::SMAA)
		{
			if (bgfx::isValid(s_main->smaaAreaTex))
				bgfx::destroyTexture(s_main->smaaAreaTex);
			if (bgfx::isValid(s_main->smaaSearchTex))
				bgfx::destroyTexture(s_main->smaaSearchTex);
		}

		s_main.reset(nullptr);
	}

	if (destroyWindow)
	{
		bgfx::shutdown();
		window::Shutdown();
	}
}

} // namespace main
} // namespace renderer
