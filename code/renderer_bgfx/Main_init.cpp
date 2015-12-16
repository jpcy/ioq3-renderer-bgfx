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

extern "C"
{
	void R_NoiseInit();
}

void QDECL Com_Printf(const char *msg, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	renderer::ri.Printf(PRINT_ALL, "%s", text);
}

void QDECL Com_Error(int level, const char *error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	renderer::ri.Error(level, "%s", text);
}

namespace renderer {

struct Backend
{
	bgfx::RendererType::Enum type;
	const char *name;
	const char *id; // Used by r_backend cvar.
};

static const Backend backends[] =
{
	{ bgfx::RendererType::Null, "Null", "null" },
	{ bgfx::RendererType::Direct3D9, "Direct3D 9.0", "d3d9" },
	{ bgfx::RendererType::Direct3D11, "Direct3D 11.0", "d3d11" },
	{ bgfx::RendererType::Direct3D12, "Direct3D 12.0", "d3d12" },
	{ bgfx::RendererType::OpenGLES, "OpenGL ES 2.0+", "gles" },
	{ bgfx::RendererType::OpenGL, "OpenGL 2.1+", "gl" },
	{ bgfx::RendererType::Vulkan, "Vulkan", "vulkan" }
};

static const size_t nBackends = ARRAY_LEN(backends);

BgfxCallback bgfxCallback;

const mat4 Main::toOpenGlMatrix
(
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
);

refimport_t ri;
glconfig_t glConfig = {};

bgfx::VertexDecl Vertex::decl;

std::unique_ptr<Main> g_main;

void WarnOnce(WarnOnceId::Enum id)
{
	static bool warned[WarnOnceId::Num];

	if (!warned[id])
	{
		ri.Printf(PRINT_WARNING, "BGFX transient buffer alloc failed\n");
		warned[id] = true;
	}
}

ConsoleVariables::ConsoleVariables()
{
	backend = ri.Cvar_Get("r_backend", "", CVAR_ARCHIVE | CVAR_LATCH);

	{
		#define FORMAT "%-10s%s\n"
		std::string description;
		description += va(FORMAT, "", "Autodetect");

		for (size_t i = 0; i < nBackends; i++)
		{
			description += va(FORMAT, backends[i].id, backends[i].name);
		}

		ri.Cvar_SetDescription(backend, description.c_str());
		#undef FORMAT
	}

	bgfx_stats = ri.Cvar_Get("r_bgfx_stats", "0", CVAR_CHEAT);
	debugText = ri.Cvar_Get("r_debugText", "0", CVAR_CHEAT);
	maxAnisotropy = ri.Cvar_Get("r_maxAnisotropy", "0", CVAR_ARCHIVE | CVAR_LATCH);
	msaa = ri.Cvar_Get("r_msaa", "4", CVAR_ARCHIVE | CVAR_LATCH);
	overBrightBits = ri.Cvar_Get ("r_overBrightBits", "1", CVAR_ARCHIVE | CVAR_LATCH);
	picmip = ri.Cvar_Get("r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(picmip, 0, 16, qtrue);
	screenshotJpegQuality = ri.Cvar_Get("r_screenshotJpegQuality", "90", CVAR_ARCHIVE);
	wireframe = ri.Cvar_Get("r_wireframe", "0", CVAR_CHEAT);

	// Window
	allowResize = ri.Cvar_Get("r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH);
	centerWindow = ri.Cvar_Get("r_centerWindow", "0", CVAR_ARCHIVE | CVAR_LATCH);
	customheight = ri.Cvar_Get("r_customheight", "1024", CVAR_ARCHIVE | CVAR_LATCH);
	customwidth = ri.Cvar_Get("r_customwidth", "1600", CVAR_ARCHIVE | CVAR_LATCH);
	customPixelAspect = ri.Cvar_Get("r_customPixelAspect", "1", CVAR_ARCHIVE | CVAR_LATCH);
	fullscreen = ri.Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE);
	mode = ri.Cvar_Get("r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH);
	noborder = ri.Cvar_Get("r_noborder", "0", CVAR_ARCHIVE | CVAR_LATCH);

	// Railgun
	railWidth = ri.Cvar_Get("r_railWidth", "16", CVAR_ARCHIVE);
	railCoreWidth = ri.Cvar_Get("r_railCoreWidth", "6", CVAR_ARCHIVE);
	railSegmentLength = ri.Cvar_Get("r_railSegmentLength", "32", CVAR_ARCHIVE);
}

static void Cmd_ReloadShaders()
{
	g_main->shaderCache.release();
	g_main->shaderCache = std::make_unique<ShaderCache>();
	g_main->shaderCache->initialize();
}

static void TakeScreenshot(const char *extension)
{
	const bool silent = !strcmp(ri.Cmd_Argv(1), "silent");
	static int lastNumber = -1;
	char filename[MAX_OSPATH];

	if (ri.Cmd_Argc() == 2 && !silent)
	{
		// Explicit filename.
		Com_sprintf(filename, MAX_OSPATH, "screenshots/%s.%s", ri.Cmd_Argv(1), extension);
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
				Com_sprintf(filename, MAX_OSPATH, "screenshots/shot9999.%s", extension);
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
				Com_sprintf(filename, MAX_OSPATH, "screenshots/shot%i%i%i%i.%s", a, b, c, d, extension);
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

Main::Main()
{
	ri.Cmd_AddCommand("r_reloadShaders", Cmd_ReloadShaders);
	ri.Cmd_AddCommand("screenshot", Cmd_Screenshot);
	ri.Cmd_AddCommand("screenshotJPEG", Cmd_ScreenshotJPEG);
	ri.Cmd_AddCommand("screenshotPNG", Cmd_ScreenshotPNG);

	sunDirection.normalize();

	// Setup the overbright lighting. Allow 2 overbright bits.
	overbrightBits = math::Clamped(cvars.overBrightBits->integer, 0, 2);
	identityLight = 1.0f / (1 << overbrightBits);
	identityLightByte = 255 * identityLight;

	// init function tables
	for (size_t i = 0; i < funcTableSize; i++)
	{
		sinTable[i] = sin(DEG2RAD(i * 360.0f / float(funcTableSize - 1)));
		squareTable[i] = (i < funcTableSize / 2) ? 1.0f : -1.0f;
		sawToothTable[i] = (float)i / funcTableSize;
		inverseSawToothTable[i] = 1.0f - sawToothTable[i];

		if (i < funcTableSize / 2)
		{
			if (i < funcTableSize / 4)
			{
				triangleTable[i] = (float) i / (funcTableSize / 4);
			}
			else
			{
				triangleTable[i] = 1.0f - triangleTable[i - funcTableSize / 4];
			}
		}
		else
		{
			triangleTable[i] = -triangleTable[i - funcTableSize / 2];
		}
	}

	R_NoiseInit();
}

Main::~Main()
{
	ri.Cmd_RemoveCommand("r_reloadShaders");
	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("screenshotJPEG");
	ri.Cmd_RemoveCommand("screenshotPNG");
}

void Main::initialize()
{
	// Create a window if we don't have one.
	if (glConfig.vidWidth == 0)
	{
		auto backend = bgfx::RendererType::Count;

		for (size_t i = 0; i < nBackends; i++)
		{
			if (!Q_stricmp(cvars.backend->string, backends[i].id))
			{
				backend = backends[i].type;
				break;
			}
		}

		Window_Initialize(backend == bgfx::RendererType::OpenGL);
		bgfx::init(backend, 0, 0, &bgfxCallback);

		for (size_t i = 0; i < nBackends; i++)
		{
			if (backends[i].type == bgfx::getCaps()->rendererType)
			{
				ri.Printf(PRINT_ALL, "Renderer backend: %s\n", backends[i].name);
				break;
			}
		}
	}

	uint32_t resetFlags = 0;

	if (cvars.maxAnisotropy->integer)
	{
		resetFlags |= BGFX_RESET_MAXANISOTROPY;
	}

	if (cvars.msaa->integer == 2)
	{
		resetFlags |= BGFX_RESET_MSAA_X2;
	}
	else if (cvars.msaa->integer == 4)
	{
		resetFlags |= BGFX_RESET_MSAA_X4;
	}
	else if (cvars.msaa->integer == 8)
	{
		resetFlags |= BGFX_RESET_MSAA_X8;
	}
	else if (cvars.msaa->integer == 16)
	{
		resetFlags |= BGFX_RESET_MSAA_X16;
	}

	bgfx::reset(glConfig.vidWidth, glConfig.vidHeight, resetFlags);
	glConfig.maxTextureSize = bgfx::getCaps()->maxTextureSize;
	Vertex::init();
	uniforms = std::make_unique<Uniforms>();
	textureCache = std::make_unique<TextureCache>();
	shaderCache = std::make_unique<ShaderCache>();
	shaderCache->initialize();
	materialCache = std::make_unique<MaterialCache>();
	modelCache = std::make_unique<ModelCache>();

	// Create fullscreen geometry.
	auto verticesMem = bgfx::alloc(sizeof(Vertex) * 4);
	auto vertices = (Vertex *)verticesMem->data;
	vertices[0].pos = { 0, 0, 0 }; vertices[0].texCoord = { 0, 0 }; vertices[0].color = vec4::white;
	vertices[1].pos = { 1, 0, 0 }; vertices[1].texCoord = { 1, 0 }; vertices[1].color = vec4::white;
	vertices[2].pos = { 1, 1, 0 }; vertices[2].texCoord = { 1, 1 }; vertices[2].color = vec4::white;
	vertices[3].pos = { 0, 1, 0 }; vertices[3].texCoord = { 0, 1 }; vertices[3].color = vec4::white;
	fsVertexBuffer_.handle = bgfx::createVertexBuffer(verticesMem, Vertex::decl);

	auto indicesMem = bgfx::alloc(sizeof(uint16_t) * 6);
	auto indices = (uint16_t *)indicesMem->data;
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 2; indices[4] = 3; indices[5] = 0;
	fsIndexBuffer_.handle = bgfx::createIndexBuffer(indicesMem);
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
#elif defined Q3_LITTLE_ENDIAN
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
	Com_sprintf(name, sizeof(name), "fonts/fontImage_%i.dat", pointSize);

	for (int i = 0; i < nFonts_; i++)
	{
		if (Q_stricmp(name, fonts_[i].name) == 0)
		{
			Com_Memcpy(font, &fonts_[i], sizeof(fontInfo_t));
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
		Q_strncpyz(font->glyphs[i].shaderName, (const char *)&data[offset], sizeof(font->glyphs[i].shaderName));
		offset += sizeof(font->glyphs[i].shaderName);
	}

	font->glyphScale = Font_ReadFloat(data, &offset);
	Q_strncpyz(font->name, name, sizeof(font->name));

	for (int i = GLYPH_START; i <= GLYPH_END; i++)
	{
		auto m = g_main->materialCache->findMaterial(font->glyphs[i].shaderName, MaterialLightmapId::StretchPic, false);
		font->glyphs[i].glyph = m->defaultShader ? 0 : m->index;
	}

	Com_Memcpy(&fonts_[nFonts_++], font, sizeof(fontInfo_t));
	ri.FS_FreeFile((void **)data);
}

static void RE_Shutdown(qboolean destroyWindow)
{
	ri.Printf(PRINT_ALL, "RE_Shutdown(%i)\n", destroyWindow);
	delete g_main.release();

	if (destroyWindow)
	{
		Window_Shutdown();
		bgfx::shutdown();
	}
}

static void RE_BeginRegistration(glconfig_t *config)
{
	ri.Printf(PRINT_ALL, "----- Renderer Init -----\n");
	g_main = std::make_unique<Main>();
	g_main->initialize();
	*config = glConfig;
}

static qhandle_t RE_RegisterModel(const char *name)
{
	auto m = g_main->modelCache->findModel(name);

	if (!m)
		return 0;

	return (qhandle_t)m->getIndex();
}

static qhandle_t RE_RegisterSkin(const char *name)
{
	auto s = g_main->materialCache->findSkin(name);

	if (!s)
		return 0;

	return s->getHandle();
}

static qhandle_t RE_RegisterShader(const char *name)
{
	auto m = g_main->materialCache->findMaterial(name);

	if (m->defaultShader)
		return 0;

	return m->index;
}

static qhandle_t RE_RegisterShaderNoMip(const char *name)
{
	auto m = g_main->materialCache->findMaterial(name, MaterialLightmapId::StretchPic, false);

	if (m->defaultShader)
		return 0;

	return m->index;
}

static void RE_LoadWorld(const char *name)
{
	g_main->loadWorld(name);
}

static void RE_SetWorldVisData(const byte *vis)
{
	g_main->externalVisData = vis;
}

static void RE_EndRegistration()
{
}

static void RE_ClearScene()
{
}

static void RE_AddRefEntityToScene(const refEntity_t *re)
{
	g_main->addEntityToScene(re);
}

static void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num)
{
	g_main->addPolyToScene(hShader, numVerts, verts, num);
}

static int RE_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir)
{
	return g_main->sampleLight(point, (vec3 *)ambientLight, (vec3 *)directedLight, (vec3 *)lightDir) ? qtrue : qfalse;
}

static void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
	DynamicLight light;
	light.position = org;
	light.color = vec4(r, g, b, 1);
	light.intensity = intensity;
	g_main->addDynamicLightToScene(light);
}

static void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
}

static void RE_RenderScene(const refdef_t *fd)
{
	g_main->renderScene(fd);
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

	g_main->setColor(c);
}

static void RE_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader)
{
	g_main->drawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
}

static void RE_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	g_main->drawStretchRaw(x, y, w, h, cols, rows, data, client, dirty == qtrue);
}

static void RE_UploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	g_main->uploadCinematic(w, h, cols, rows, data, client, dirty == qtrue);
}

static void RE_BeginFrame(stereoFrame_t stereoFrame)
{
}

static void RE_EndFrame(int *frontEndMsec, int *backEndMsec)
{
	g_main->endFrame();
}

static int RE_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)
{
	if (g_main->world)
		return g_main->world->markFragments(numPoints, points, projection, maxPoints, pointBuffer, maxFragments, fragmentBuffer);

	return 0;
}

static int RE_LerpTag(orientation_t *orientation, qhandle_t handle, int startFrame, int endFrame, float frac, const char *tagName)
{
	auto m = g_main->modelCache->getModel(handle);
	auto from = m->getTag(tagName, startFrame);
	auto to = m->getTag(tagName, endFrame);
	const float invFrac = 1.0f - frac;

	Transform lerped;
	lerped.position = vec3::lerp(from.position, to.position, frac);
	lerped.rotation[0] = vec3::lerp(from.rotation[0], to.rotation[0], frac).normal();
	lerped.rotation[1] = vec3::lerp(from.rotation[1], to.rotation[1], frac).normal();
	lerped.rotation[2] = vec3::lerp(from.rotation[2], to.rotation[2], frac).normal();

	VectorCopy(lerped.position, orientation->origin);
	VectorCopy(lerped.rotation[0], orientation->axis[0]);
	VectorCopy(lerped.rotation[1], orientation->axis[1]);
	VectorCopy(lerped.rotation[2], orientation->axis[2]);
	return qtrue;
}

static void RE_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs)
{
	auto m = g_main->modelCache->getModel(handle);
	auto bounds = m->getBounds();
	VectorCopy(bounds[0], mins);
	VectorCopy(bounds[1], maxs);
}

static void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	g_main->registerFont(fontName, pointSize, font);
}

static void RE_RemapShader(const char *oldShader, const char *newShader, const char *offsetTime)
{
	g_main->materialCache->remapMaterial(oldShader, newShader, offsetTime);
}

static qboolean RE_GetEntityToken(char *buffer, int size)
{
	if (g_main->world)
	{
		return g_main->world->getEntityToken(buffer, size) ? qtrue : qfalse;
	}

	return qfalse;
}

static qboolean RE_inPVS( const vec3_t p1, const vec3_t p2)
{
	return qtrue;
}

static void RE_TakeVideoFrame( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg)
{
}

#ifdef USE_RENDERER_DLOPEN
extern "C" Q_EXPORT refexport_t * QDECL GetRefAPI(int apiVersion, refimport_t *rimp)
{
#else
extern "C" refexport_t *GetRefAPI(int apiVersion, refimport_t *rimp)
{
#endif
	ri = *rimp;

	static refexport_t re;
	Com_Memset(&re, 0, sizeof(re));

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
