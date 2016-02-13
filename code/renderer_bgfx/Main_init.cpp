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
	ri.Cvar_SetDescription(aa,
		"<empty>   None\n"
		"fxaa      Fast Approximate Anti-Aliasing (FXAA v2)\n");
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
		"<empty>   None\n"
		"depth     Linear depth\n"
		"lum       Average and adapted luminance\n");
	debugDrawSize = ri.Cvar_Get("r_debugDrawSize", "256", CVAR_ARCHIVE);
	debugText = ri.Cvar_Get("r_debugText", "0", CVAR_CHEAT);
	maxAnisotropy = ri.Cvar_Get("r_maxAnisotropy", "0", CVAR_ARCHIVE | CVAR_LATCH);
	picmip = ri.Cvar_Get("r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(picmip, 0, 16, qtrue);
	softSprites = ri.Cvar_Get("r_softSprites", "1", CVAR_ARCHIVE);
	screenshotJpegQuality = ri.Cvar_Get("r_screenshotJpegQuality", "90", CVAR_ARCHIVE);
	wireframe = ri.Cvar_Get("r_wireframe", "0", CVAR_CHEAT);

	// HDR
	hdr = ri.Cvar_Get("r_hdr", "0", CVAR_ARCHIVE | CVAR_LATCH);
	hdrKey = ri.Cvar_Get("r_hdrKey", "0.1", CVAR_ARCHIVE);

	// Railgun
	railWidth = ri.Cvar_Get("r_railWidth", "16", CVAR_ARCHIVE);
	railCoreWidth = ri.Cvar_Get("r_railCoreWidth", "6", CVAR_ARCHIVE);
	railSegmentLength = ri.Cvar_Get("r_railSegmentLength", "32", CVAR_ARCHIVE);

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

	debugDraw_ = DebugDrawFromString(g_cvars.debugDraw->string);
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
	fragMem[FragmentShaderId::AdaptedLuminance] = MR(AdaptedLuminance_fragment_##backend);                       \
	fragMem[FragmentShaderId::Depth] = MR(Depth_fragment_##backend);                                             \
	fragMem[FragmentShaderId::Depth_AlphaTest] = MR(Depth_AlphaTest_fragment_##backend);                         \
	fragMem[FragmentShaderId::Fog] = MR(Fog_fragment_##backend);                                                 \
	fragMem[FragmentShaderId::FXAA] = MR(FXAA_fragment_##backend);                                               \
	fragMem[FragmentShaderId::Generic] = MR(Generic_fragment_##backend);                                         \
	fragMem[FragmentShaderId::Generic_AlphaTest] = MR(Generic_AlphaTest_fragment_##backend);                     \
	fragMem[FragmentShaderId::Generic_AlphaTestSoftSprite] = MR(Generic_AlphaTestSoftSprite_fragment_##backend); \
	fragMem[FragmentShaderId::Generic_SoftSprite] = MR(Generic_SoftSprite_fragment_##backend);                   \
	fragMem[FragmentShaderId::LinearDepth] = MR(LinearDepth_fragment_##backend);                                 \
	fragMem[FragmentShaderId::Luminance] = MR(Luminance_fragment_##backend);                                     \
	fragMem[FragmentShaderId::LuminanceDownsample] = MR(LuminanceDownsample_fragment_##backend);                 \
	fragMem[FragmentShaderId::Texture] = MR(Texture_fragment_##backend);                                         \
	fragMem[FragmentShaderId::TextureColor] = MR(TextureColor_fragment_##backend);                               \
	fragMem[FragmentShaderId::TextureSingleChannel] = MR(TextureSingleChannel_fragment_##backend);			     \
	fragMem[FragmentShaderId::ToneMap] = MR(ToneMap_fragment_##backend);                                         \
	vertMem[VertexShaderId::Depth] = MR(Depth_vertex_##backend);                                                 \
	vertMem[VertexShaderId::Depth_AlphaTest] = MR(Depth_AlphaTest_vertex_##backend);                             \
	vertMem[VertexShaderId::Fog] = MR(Fog_vertex_##backend);                                                     \
	vertMem[VertexShaderId::Generic] = MR(Generic_vertex_##backend);                                             \
	vertMem[VertexShaderId::Texture] = MR(Texture_vertex_##backend);

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
	fragMap[ShaderProgramId::AdaptedLuminance]            = FragmentShaderId::AdaptedLuminance;
	fragMap[ShaderProgramId::Depth]                       = FragmentShaderId::Depth;
	fragMap[ShaderProgramId::Depth_AlphaTest]             = FragmentShaderId::Depth_AlphaTest;
	fragMap[ShaderProgramId::Fog]                         = FragmentShaderId::Fog;
	fragMap[ShaderProgramId::FXAA]                        = FragmentShaderId::FXAA;
	fragMap[ShaderProgramId::Generic]                     = FragmentShaderId::Generic;
	fragMap[ShaderProgramId::Generic_AlphaTest]           = FragmentShaderId::Generic_AlphaTest;
	fragMap[ShaderProgramId::Generic_AlphaTestSoftSprite] = FragmentShaderId::Generic_AlphaTestSoftSprite;
	fragMap[ShaderProgramId::Generic_SoftSprite]          = FragmentShaderId::Generic_SoftSprite;
	fragMap[ShaderProgramId::LinearDepth]                 = FragmentShaderId::LinearDepth;
	fragMap[ShaderProgramId::Luminance]                   = FragmentShaderId::Luminance;
	fragMap[ShaderProgramId::LuminanceDownsample]         = FragmentShaderId::LuminanceDownsample;
	fragMap[ShaderProgramId::Texture]                     = FragmentShaderId::Texture;
	fragMap[ShaderProgramId::TextureColor]                = FragmentShaderId::TextureColor;
	fragMap[ShaderProgramId::TextureSingleChannel]        = FragmentShaderId::TextureSingleChannel;
	fragMap[ShaderProgramId::ToneMap] = FragmentShaderId::ToneMap;
	std::array<VertexShaderId::Enum, ShaderProgramId::Num> vertMap;
	vertMap[ShaderProgramId::AdaptedLuminance]            = VertexShaderId::Texture;
	vertMap[ShaderProgramId::Depth]                       = VertexShaderId::Depth;
	vertMap[ShaderProgramId::Depth_AlphaTest]             = VertexShaderId::Depth_AlphaTest;
	vertMap[ShaderProgramId::Fog]                         = VertexShaderId::Fog;
	vertMap[ShaderProgramId::FXAA]                        = VertexShaderId::Texture;
	vertMap[ShaderProgramId::Generic]                     = VertexShaderId::Generic;
	vertMap[ShaderProgramId::Generic_AlphaTest]           = VertexShaderId::Generic;
	vertMap[ShaderProgramId::Generic_AlphaTestSoftSprite] = VertexShaderId::Generic;
	vertMap[ShaderProgramId::Generic_SoftSprite]          = VertexShaderId::Generic;
	vertMap[ShaderProgramId::LinearDepth]                 = VertexShaderId::Texture;
	vertMap[ShaderProgramId::Luminance]                   = VertexShaderId::Texture;
	vertMap[ShaderProgramId::LuminanceDownsample]         = VertexShaderId::Texture;
	vertMap[ShaderProgramId::Texture]                     = VertexShaderId::Texture;
	vertMap[ShaderProgramId::TextureColor]                = VertexShaderId::Texture;
	vertMap[ShaderProgramId::TextureSingleChannel]        = VertexShaderId::Texture;
	vertMap[ShaderProgramId::ToneMap]                     = VertexShaderId::Texture;

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
	sceneFbColor_ = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, 1, g_cvars.hdr->integer != 0 ? bgfx::TextureFormat::RGBA16F : bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP);
	sceneFbDepth_ = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, 1, bgfx::TextureFormat::D24, BGFX_TEXTURE_RT);
	bgfx::TextureHandle sceneTextures[] = { sceneFbColor_, sceneFbDepth_ };
	sceneFb_.handle = bgfx::createFrameBuffer(2, sceneTextures, true);

	if (g_cvars.hdr->integer != 0)
	{
		// Luminance frame buffers.
		for (size_t i = 0; i < nLuminanceFrameBuffers_; i++)
		{
			luminanceFrameBuffers_[i].handle = bgfx::createFrameBuffer(luminanceFrameBufferSizes_[i], luminanceFrameBufferSizes_[i], bgfx::TextureFormat::R16F);
		}

		adaptedLuminanceFB_[0].handle = bgfx::createFrameBuffer(1, 1, bgfx::TextureFormat::R16F);
		adaptedLuminanceFB_[1].handle = bgfx::createFrameBuffer(1, 1, bgfx::TextureFormat::R16F);
	}

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

void AddDynamicLightToScene(DynamicLight light)
{
	s_main->addDynamicLightToScene(light);
}

void AddEntityToScene(const refEntity_t *entity)
{
	return s_main->addEntityToScene(entity);
}

void AddPolyToScene(qhandle_t hShader, int nVerts, const polyVert_t *verts, int nPolys)
{
	return s_main->addPolyToScene(hShader, nVerts, verts, nPolys);
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

bool IsMirrorCamera()
{
	return s_main->isMirrorCamera();
}

void LoadWorld(const char *name)
{
	s_main->loadWorld(name);
}

void OnMaterialCreate(Material *material)
{
	s_main->onMaterialCreate(material);
}

void OnModelCreate(Model *model)
{
	s_main->onModelCreate(model);
}

void RegisterFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	s_main->registerFont(fontName, pointSize, font);
}

void RenderScene(const refdef_t *def)
{
	s_main->renderScene(def);
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
