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

#include "../../build/Shaders.h"
#include "Main.h"

namespace renderer {

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
glconfig_t glConfig = {};

bgfx::VertexDecl Vertex::decl;

static std::unique_ptr<Main> s_main;

ConsoleVariables g_cvars;
const uint8_t *g_externalVisData = nullptr;
MaterialCache *g_materialCache = nullptr;
ModelCache *g_modelCache = nullptr;
TextureCache *g_textureCache = nullptr;

float g_sinTable[g_funcTableSize];
float g_squareTable[g_funcTableSize];
float g_triangleTable[g_funcTableSize];
float g_sawToothTable[g_funcTableSize];
float g_inverseSawToothTable[g_funcTableSize];

void WarnOnce(WarnOnceId::Enum id)
{
	static bool warned[WarnOnceId::Num];

	if (!warned[id])
	{
		ri.Printf(PRINT_WARNING, "BGFX transient buffer alloc failed\n");
		warned[id] = true;
	}
}

void ConsoleVariables::initialize()
{
	aa = ri.Cvar_Get("r_aa", "", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_SetDescription(aa, "<empty>   None\nfxaa      Fast Approximate Anti-Aliasing (FXAA v2)\n");
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
	debugText = ri.Cvar_Get("r_debugText", "0", CVAR_CHEAT);
	maxAnisotropy = ri.Cvar_Get("r_maxAnisotropy", "0", CVAR_ARCHIVE | CVAR_LATCH);
	picmip = ri.Cvar_Get("r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(picmip, 0, 16, qtrue);
	softSprites = ri.Cvar_Get("r_softSprites", "1", CVAR_ARCHIVE);
	screenshotJpegQuality = ri.Cvar_Get("r_screenshotJpegQuality", "90", CVAR_ARCHIVE);
	showDepth = ri.Cvar_Get("r_showDepth", "0", CVAR_ARCHIVE | CVAR_CHEAT);
	wireframe = ri.Cvar_Get("r_wireframe", "0", CVAR_CHEAT);

	// Gamma
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

	// Railgun
	railWidth = ri.Cvar_Get("r_railWidth", "16", CVAR_ARCHIVE);
	railCoreWidth = ri.Cvar_Get("r_railCoreWidth", "6", CVAR_ARCHIVE);
	railSegmentLength = ri.Cvar_Get("r_railSegmentLength", "32", CVAR_ARCHIVE);
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

Main::Main()
{
	g_cvars.initialize();
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
}

Main::~Main()
{
	for (size_t i = 0; i < maxDynamicLightTextures_; i++)
	{
		bgfx::destroyTexture(dynamicLightsTextures_[i]);
	}

	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("screenshotJPEG");
	ri.Cmd_RemoveCommand("screenshotPNG");
	g_materialCache = nullptr;
	g_modelCache = nullptr;
	g_textureCache = nullptr;
}

void Main::initialize()
{
	// Create a window if we don't have one.
	if (glConfig.vidWidth == 0)
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

		Window_Initialize(selectedBackend == bgfx::RendererType::OpenGL);
		
		if (!bgfx::init(selectedBackend, 0, 0, &bgfxCallback))
		{
			ri.Error(ERR_DROP, "bgfx init failed");
		}

		// Print the chosen backend name. It may not be the one that was selected.
		const bool forced = selectedBackend != bgfx::RendererType::Count && selectedBackend != bgfx::getCaps()->rendererType;
		ri.Printf(PRINT_ALL, "Renderer backend%s: %s\n", forced ? " forced to" : "", bgfx::getRendererName(bgfx::getCaps()->rendererType));
	}

	uint32_t resetFlags = 0;

	if (g_cvars.maxAnisotropy->integer)
	{
		resetFlags |= BGFX_RESET_MAXANISOTROPY;
	}

	bgfx::reset(glConfig.vidWidth, glConfig.vidHeight, resetFlags);
	const bgfx::Caps *caps = bgfx::getCaps();

	if (bgfx::getCaps()->maxFBAttachments < 2)
	{
		ri.Error(ERR_DROP, "MRT not supported.\n");
	}

	halfTexelOffset_ = caps->rendererType == bgfx::RendererType::Direct3D9 ? 0.5f : 0;
	isTextureOriginBottomLeft_ = caps->rendererType == bgfx::RendererType::OpenGL || caps->rendererType == bgfx::RendererType::OpenGLES;
	glConfig.deviceSupportsGamma = qtrue;
	glConfig.maxTextureSize = bgfx::getCaps()->maxTextureSize;
	Vertex::init();
	uniforms_ = std::make_unique<Uniforms>();
	entityUniforms_ = std::make_unique<Uniforms_Entity>();
	matUniforms_ = std::make_unique<Uniforms_Material>();
	matStageUniforms_ = std::make_unique<Uniforms_MaterialStage>();
	textureCache_ = std::make_unique<TextureCache>();
	g_textureCache = textureCache_.get();
	materialCache_ = std::make_unique<MaterialCache>();
	g_materialCache = materialCache_.get();
	modelCache_ = std::make_unique<ModelCache>();
	g_modelCache = modelCache_.get();

	// Map shader ids to shader sources.
	std::array<const bgfx::Memory *, FragmentShaderId::Num> fragMem;
	std::array<const bgfx::Memory *, VertexShaderId::Num> vertMem;
	#define MR(name) bgfx::makeRef(name, sizeof(name))
	#define SHADER_MEM(backend) \
	fragMem[FragmentShaderId::Depth] = MR(Depth_fragment_##backend);                                             \
	fragMem[FragmentShaderId::Depth_AlphaTest] = MR(Depth_AlphaTest_fragment_##backend);                         \
	fragMem[FragmentShaderId::Fog] = MR(Fog_fragment_##backend);                                                 \
	fragMem[FragmentShaderId::Fullscreen_Blit] = MR(Fullscreen_Blit_fragment_##backend);                         \
	fragMem[FragmentShaderId::Fullscreen_FXAA] = MR(Fullscreen_FXAA_fragment_##backend);                         \
	fragMem[FragmentShaderId::Fullscreen_LinearDepth] = MR(Fullscreen_LinearDepth_fragment_##backend);           \
	fragMem[FragmentShaderId::Fullscreen_ToneMap] = MR(Fullscreen_ToneMap_fragment_##backend);                   \
	fragMem[FragmentShaderId::Generic] = MR(Generic_fragment_##backend);                                         \
	fragMem[FragmentShaderId::Generic_AlphaTest] = MR(Generic_AlphaTest_fragment_##backend);                     \
	fragMem[FragmentShaderId::Generic_AlphaTestSoftSprite] = MR(Generic_AlphaTestSoftSprite_fragment_##backend); \
	fragMem[FragmentShaderId::Generic_SoftSprite] = MR(Generic_SoftSprite_fragment_##backend);                   \
	fragMem[FragmentShaderId::TextureColor] = MR(TextureColor_fragment_##backend);                               \
	vertMem[VertexShaderId::Depth] = MR(Depth_vertex_##backend);                                                 \
	vertMem[VertexShaderId::Depth_AlphaTest] = MR(Depth_AlphaTest_vertex_##backend);                             \
	vertMem[VertexShaderId::Fog] = MR(Fog_vertex_##backend);                                                     \
	vertMem[VertexShaderId::Fullscreen] = MR(Fullscreen_vertex_##backend);                                       \
	vertMem[VertexShaderId::Generic] = MR(Generic_vertex_##backend);                                             \
	vertMem[VertexShaderId::TextureColor] = MR(TextureColor_vertex_##backend);

	if (caps->rendererType == bgfx::RendererType::OpenGL)
	{
		SHADER_MEM(gl);
	}
#ifdef WIN32
	else if (caps->rendererType == bgfx::RendererType::Direct3D11)
	{
		SHADER_MEM(d3d11)
	}
#endif

	// Map shader programs to their vertex and fragment shaders.
	std::array<FragmentShaderId::Enum, ShaderProgramId::Num> fragMap;
	fragMap[ShaderProgramId::Depth]                       = FragmentShaderId::Depth;
	fragMap[ShaderProgramId::Depth_AlphaTest]             = FragmentShaderId::Depth_AlphaTest;
	fragMap[ShaderProgramId::Fog]                         = FragmentShaderId::Fog;
	fragMap[ShaderProgramId::Fullscreen_Blit]             = FragmentShaderId::Fullscreen_Blit;
	fragMap[ShaderProgramId::Fullscreen_FXAA]             = FragmentShaderId::Fullscreen_FXAA;
	fragMap[ShaderProgramId::Fullscreen_LinearDepth]      = FragmentShaderId::Fullscreen_LinearDepth;
	fragMap[ShaderProgramId::Fullscreen_ToneMap]          = FragmentShaderId::Fullscreen_ToneMap;
	fragMap[ShaderProgramId::Generic]                     = FragmentShaderId::Generic;
	fragMap[ShaderProgramId::Generic_AlphaTest]           = FragmentShaderId::Generic_AlphaTest;
	fragMap[ShaderProgramId::Generic_AlphaTestSoftSprite] = FragmentShaderId::Generic_AlphaTestSoftSprite;
	fragMap[ShaderProgramId::Generic_SoftSprite]          = FragmentShaderId::Generic_SoftSprite;
	fragMap[ShaderProgramId::TextureColor]                = FragmentShaderId::TextureColor;
	std::array<VertexShaderId::Enum, ShaderProgramId::Num> vertMap;
	vertMap[ShaderProgramId::Depth]                       = VertexShaderId::Depth;
	vertMap[ShaderProgramId::Depth_AlphaTest]             = VertexShaderId::Depth_AlphaTest;
	vertMap[ShaderProgramId::Fog]                         = VertexShaderId::Fog;
	vertMap[ShaderProgramId::Fullscreen_Blit]             = VertexShaderId::Fullscreen;
	vertMap[ShaderProgramId::Fullscreen_FXAA]             = VertexShaderId::Fullscreen;
	vertMap[ShaderProgramId::Fullscreen_LinearDepth]      = VertexShaderId::Fullscreen;
	vertMap[ShaderProgramId::Fullscreen_ToneMap]          = VertexShaderId::Fullscreen;
	vertMap[ShaderProgramId::Generic]                     = VertexShaderId::Generic;
	vertMap[ShaderProgramId::Generic_AlphaTest]           = VertexShaderId::Generic;
	vertMap[ShaderProgramId::Generic_AlphaTestSoftSprite] = VertexShaderId::Generic;
	vertMap[ShaderProgramId::Generic_SoftSprite]          = VertexShaderId::Generic;
	vertMap[ShaderProgramId::TextureColor]                = VertexShaderId::TextureColor;

	for (size_t i = 0; i < ShaderProgramId::Num; i++)
	{
		auto &fragment = fragmentShaders_[fragMap[i]];

		if (!bgfx::isValid(fragment.handle))
		{
			fragment.handle = bgfx::createShader(fragMem[fragMap[i]]);

			if (!bgfx::isValid(fragment.handle))
				ri.Error(ERR_DROP, "Error creating fragment shader");
		}

		auto &vertex = vertexShaders_[vertMap[i]];
	
		if (!bgfx::isValid(vertex.handle))
		{
			vertex.handle = bgfx::createShader(vertMem[vertMap[i]]);

			if (!bgfx::isValid(vertex.handle))
				ri.Error(ERR_DROP, "Error creating vertex shader");
		}

		shaderPrograms_[i].handle = bgfx::createProgram(vertex.handle, fragment.handle);

		if (!bgfx::isValid(shaderPrograms_[i].handle))
			ri.Error(ERR_DROP, "Error creating shader program");
	}

	// Parse anti-aliasing cvar.
	aa_ = util::Stricmp(g_cvars.aa->string, "fxaa") == 0 ? AntiAliasing::FXAA : AntiAliasing::None;

	// FXAA frame buffer.
	if (aa_ == AntiAliasing::FXAA)
	{
		fxaaColor_ = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP);
		fxaaFb_.handle = bgfx::createFrameBuffer(1, &fxaaColor_, true);
	}

	// Linear depth frame buffer.
	linearDepthFb_.handle = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Equal, bgfx::TextureFormat::R16F);

	// Scene frame buffer.
	sceneFbColor_ = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP);
	sceneFbDepth_ = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, 1, bgfx::TextureFormat::D24, BGFX_TEXTURE_RT);
	bgfx::TextureHandle sceneTextures[] = { sceneFbColor_, sceneFbDepth_ };
	sceneFb_.handle = bgfx::createFrameBuffer(2, sceneTextures, true);

	// Dynamic lights.
	// Calculate the smallest square POT texture size to fit the dynamic lights data.
	const int sr = (int)ceil(sqrtf(maxDynamicLights_ * sizeof(DynamicLight) / 4.0f));
	dynamicLightTextureSize_ = 1;

	while (dynamicLightTextureSize_ < sr)
		dynamicLightTextureSize_ *= 2;

	dynamicLightTextureSize_ = (int)pow(dynamicLightTextureSize_, 2);

	for (size_t i = 0; i < maxDynamicLightTextures_; i++)
	{
		dynamicLightsTextures_[i] = bgfx::createTexture2D(dynamicLightTextureSize_, dynamicLightTextureSize_, 1, bgfx::TextureFormat::RGBA32F);
	}
}

static int Font_ReadInt(const uint8_t *data, int *offset)
{
	assert(data && offset);
	int i = data[*offset]+(data[*offset+1]<<8)+(data[*offset+2]<<16)+(data[*offset+3]<<24);
	*offset += 4;
	return i;
}

static float Font_ReadFloat(const uint8_t *data, int *offset)
{
	assert(data && offset);
	uint8_t temp[4];
#if defined Q3_BIG_ENDIAN
	temp[0] = data[*offset+3];
	temp[1] = data[*offset+2];
	temp[2] = data[*offset+1];
	temp[3] = data[*offset+0];
#else
	temp[0] = data[*offset+0];
	temp[1] = data[*offset+1];
	temp[2] = data[*offset+2];
	temp[3] = data[*offset+3];
#endif
	*offset += 4;
	return *((float *)temp);
}

void Main::registerFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	if (!fontName)
	{
		ri.Printf(PRINT_ALL, "RE_RegisterFont: called with empty name\n");
		return;
	}

	if (pointSize <= 0)
		pointSize = 12;

	if (nFonts_ >= maxFonts_)
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Too many fonts registered already.\n");
		return;
	}

	char name[1024];
	util::Sprintf(name, sizeof(name), "fonts/fontImage_%i.dat", pointSize);

	for (int i = 0; i < nFonts_; i++)
	{
		if (util::Stricmp(name, fonts_[i].name) == 0)
		{
			memcpy(font, &fonts_[i], sizeof(fontInfo_t));
			return;
		}
	}

	int len = ri.FS_ReadFile(name, NULL);

	if (len != sizeof(fontInfo_t))
		return;

	int offset = 0;
	const uint8_t *data;
	ri.FS_ReadFile(name, (void **)&data);

	for(int i = 0; i < GLYPHS_PER_FONT; i++)
	{
		font->glyphs[i].height		= Font_ReadInt(data, &offset);
		font->glyphs[i].top			= Font_ReadInt(data, &offset);
		font->glyphs[i].bottom		= Font_ReadInt(data, &offset);
		font->glyphs[i].pitch		= Font_ReadInt(data, &offset);
		font->glyphs[i].xSkip		= Font_ReadInt(data, &offset);
		font->glyphs[i].imageWidth	= Font_ReadInt(data, &offset);
		font->glyphs[i].imageHeight = Font_ReadInt(data, &offset);
		font->glyphs[i].s			= Font_ReadFloat(data, &offset);
		font->glyphs[i].t			= Font_ReadFloat(data, &offset);
		font->glyphs[i].s2			= Font_ReadFloat(data, &offset);
		font->glyphs[i].t2			= Font_ReadFloat(data, &offset);
		font->glyphs[i].glyph		= Font_ReadInt(data, &offset);
		util::Strncpyz(font->glyphs[i].shaderName, (const char *)&data[offset], sizeof(font->glyphs[i].shaderName));
		offset += sizeof(font->glyphs[i].shaderName);
	}

	font->glyphScale = Font_ReadFloat(data, &offset);
	util::Strncpyz(font->name, name, sizeof(font->name));

	for (int i = GLYPH_START; i <= GLYPH_END; i++)
	{
		auto m = materialCache_->findMaterial(font->glyphs[i].shaderName, MaterialLightmapId::StretchPic, false);
		font->glyphs[i].glyph = m->defaultShader ? 0 : m->index;
	}

	memcpy(&fonts_[nFonts_++], font, sizeof(fontInfo_t));
	ri.FS_FreeFile((void **)data);
}

namespace main {

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

bool IsMirrorCamera()
{
	return s_main->isMirrorCamera();
}

void SetSunLight(const SunLight &sunLight)
{
	s_main->setSunLight(sunLight);
}

void OnMaterialCreate(Material *material)
{
	s_main->onMaterialCreate(material);
}

void OnModelCreate(Model *model)
{
	s_main->onModelCreate(model);
}

} // namespace main

static void RE_Shutdown(qboolean destroyWindow)
{
	ri.Printf(PRINT_ALL, "RE_Shutdown(%i)\n", destroyWindow);
	world::Unload();
	s_main.reset(nullptr);

	if (destroyWindow)
	{
		bgfx::shutdown();
		Window_Shutdown();
	}
}

static void RE_BeginRegistration(glconfig_t *config)
{
	ri.Printf(PRINT_ALL, "----- Renderer Init -----\n");
	s_main = std::make_unique<Main>();
	s_main->initialize();
	*config = glConfig;
}

static qhandle_t RE_RegisterModel(const char *name)
{
	auto m = g_modelCache->findModel(name);

	if (!m)
		return 0;

	return (qhandle_t)m->getIndex();
}

static qhandle_t RE_RegisterSkin(const char *name)
{
	auto s = g_materialCache->findSkin(name);

	if (!s)
		return 0;

	return s->getHandle();
}

static qhandle_t RE_RegisterShader(const char *name)
{
	auto m = g_materialCache->findMaterial(name);

	if (m->defaultShader)
		return 0;

	return m->index;
}

static qhandle_t RE_RegisterShaderNoMip(const char *name)
{
	auto m = g_materialCache->findMaterial(name, MaterialLightmapId::StretchPic, false);

	if (m->defaultShader)
		return 0;

	return m->index;
}

static void RE_LoadWorld(const char *name)
{
	s_main->loadWorld(name);
}

static void RE_SetWorldVisData(const byte *vis)
{
	g_externalVisData = vis;
}

static void RE_EndRegistration()
{
}

static void RE_ClearScene()
{
}

static void RE_AddRefEntityToScene(const refEntity_t *re)
{
	s_main->addEntityToScene(re);
}

static void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num)
{
	s_main->addPolyToScene(hShader, numVerts, verts, num);
}

static int RE_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir)
{
	return s_main->sampleLight(point, (vec3 *)ambientLight, (vec3 *)directedLight, (vec3 *)lightDir) ? qtrue : qfalse;
}

static void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
	DynamicLight light;
	light.position_type = vec4(org, DynamicLight::Point);
	light.color_radius = vec4(r, g, b, intensity);
	s_main->addDynamicLightToScene(light);
}

static void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
}

static void RE_RenderScene(const refdef_t *fd)
{
	s_main->renderScene(fd);
}

static void RE_SetColor(const float *rgba)
{
	vec4 c;

	if (rgba == NULL)
	{
		c = vec4::white;
	}
	else
	{
		c = vec4(rgba);
	}

	s_main->setColor(c);
}

static void RE_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader)
{
	s_main->drawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
}

static void RE_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	s_main->drawStretchRaw(x, y, w, h, cols, rows, data, client, dirty == qtrue);
}

static void RE_UploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	s_main->uploadCinematic(w, h, cols, rows, data, client, dirty == qtrue);
}

static void RE_BeginFrame(stereoFrame_t stereoFrame)
{
}

static void RE_EndFrame(int *frontEndMsec, int *backEndMsec)
{
	s_main->endFrame();
}

static int RE_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)
{
	if (world::IsLoaded())
		return world::MarkFragments(numPoints, (const vec3 *)points, projection, maxPoints, (vec3 *)pointBuffer, maxFragments, fragmentBuffer);

	return 0;
}

static int RE_LerpTag(orientation_t *orientation, qhandle_t handle, int startFrame, int endFrame, float frac, const char *tagName)
{
	auto m = g_modelCache->getModel(handle);
	auto from = m->getTag(tagName, startFrame);
	auto to = m->getTag(tagName, endFrame);

	Transform lerped;
	lerped.position = vec3::lerp(from.position, to.position, frac);
	lerped.rotation[0] = vec3::lerp(from.rotation[0], to.rotation[0], frac).normal();
	lerped.rotation[1] = vec3::lerp(from.rotation[1], to.rotation[1], frac).normal();
	lerped.rotation[2] = vec3::lerp(from.rotation[2], to.rotation[2], frac).normal();

	memcpy(orientation->origin, &lerped.position.x, sizeof(vec3_t));
	memcpy(orientation->axis[0], &lerped.rotation[0].x, sizeof(vec3_t));
	memcpy(orientation->axis[1], &lerped.rotation[1].x, sizeof(vec3_t));
	memcpy(orientation->axis[2], &lerped.rotation[2].x, sizeof(vec3_t));
	return qtrue;
}

static void RE_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs)
{
	auto m = g_modelCache->getModel(handle);
	auto bounds = m->getBounds();
	memcpy(mins, &bounds.min.x, sizeof(vec3_t));
	memcpy(maxs, &bounds.max.x, sizeof(vec3_t));
}

static void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	s_main->registerFont(fontName, pointSize, font);
}

static void RE_RemapShader(const char *oldShader, const char *newShader, const char *offsetTime)
{
	g_materialCache->remapMaterial(oldShader, newShader, offsetTime);
}

static qboolean RE_GetEntityToken(char *buffer, int size)
{
	if (world::IsLoaded() && world::GetEntityToken(buffer, size))
		return qtrue;

	return qfalse;
}

static qboolean RE_inPVS( const vec3_t p1, const vec3_t p2)
{
	if (world::IsLoaded() && world::InPvs(p1, p2))
		return qtrue;

	return qfalse;
}

static void RE_TakeVideoFrame( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg)
{
}

extern "C" Q_EXPORT refexport_t * QDECL GetRefAPI(int apiVersion, refimport_t *rimp)
{
	ri = *rimp;

	static refexport_t re;
	memset(&re, 0, sizeof(re));

	if (apiVersion != REF_API_VERSION )
	{
		ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion);
		return NULL;
	}

	re.Shutdown = RE_Shutdown;
	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.LoadWorld = RE_LoadWorld;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;
	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;
	re.MarkFragments = RE_MarkFragments;
	re.LerpTag = RE_LerpTag;
	re.ModelBounds = RE_ModelBounds;
	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = RE_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.RenderScene = RE_RenderScene;
	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_DrawStretchPic;
	re.DrawStretchRaw = RE_DrawStretchRaw;
	re.UploadCinematic = RE_UploadCinematic;
	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = RE_RemapShader;
	re.GetEntityToken = RE_GetEntityToken;
	re.inPVS = RE_inPVS;
	re.TakeVideoFrame = RE_TakeVideoFrame;

	return &re;
}

} // namespace renderer
