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

namespace renderer {

#define	BIG_INFO_STRING		8192  // used for system info key only
#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString

	/*
	==========================================================

	CVARS (console variables)

	Many variables can be used for cheating purposes, so when
	cheats is zero, force all unspecified variables to their
	default values.
	==========================================================
	*/

#define	CVAR_ARCHIVE		0x0001	// set to cause it to be saved to vars.rc
	// used for system variables, not for player
	// specific configurations
#define	CVAR_USERINFO		0x0002	// sent to server on connect or change
#define	CVAR_SERVERINFO		0x0004	// sent in response to front end requests
#define	CVAR_SYSTEMINFO		0x0008	// these cvars will be duplicated on all clients
#define	CVAR_INIT		0x0010	// don't allow change from console at all,
	// but can be set from the command line
#define	CVAR_LATCH		0x0020	// will only change when C code next does
	// a Cvar_Get(), so it can't be changed
	// without proper initialization.  modified
	// will be set, even though the value hasn't
	// changed yet
#define	CVAR_ROM		0x0040	// display only, cannot be set by user at all
#define	CVAR_USER_CREATED	0x0080	// created by a set command
#define	CVAR_TEMP		0x0100	// can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT		0x0200	// can not be changed if cheats are disabled
#define CVAR_NORESTART		0x0400	// do not clear when a cvar_restart is issued

#define CVAR_SERVER_CREATED	0x0800	// cvar was created by a server the client connected to.
#define CVAR_VM_CREATED		0x1000	// cvar was created exclusively in one of the VMs.
#define CVAR_PROTECTED		0x2000	// prevent modifying this var from VMs or the server
	// These flags are only returned by the Cvar_Flags() function
#define CVAR_MODIFIED		0x40000000	// Cvar was modified
#define CVAR_NONEXISTENT	0x80000000	// Cvar doesn't exist.

// nothing outside the Cvar_*() functions should modify these fields!
struct cvar_t {
	char			*name;
	char			*string;
	char			*resetString;		// cvar_restart will reset to this value
	char			*latchedString;		// for CVAR_LATCH vars
	int				flags;
	qboolean	modified;			// set each time the cvar is changed
	int				modificationCount;	// incremented each time the cvar is changed
	float			value;				// atof( string )
	int				integer;			// atoi( string )
	qboolean	validate;
	qboolean	integral;
	float			min;
	float			max;
	char			*description;

	cvar_t *next;
	cvar_t *prev;
	cvar_t *hashNext;
	cvar_t *hashPrev;
	int			hashIndex;
};

void ConsoleVariable::checkRange(float minValue, float maxValue, bool shouldBeIntegral)
{
	ri.Cvar_CheckRange(cvar, minValue, maxValue, shouldBeIntegral ? qtrue : qfalse);
}

void ConsoleVariable::clearModified()
{
	cvar->modified = qfalse;
}

bool ConsoleVariable::getBool() const
{
	return cvar->integer != 0;
}

const char *ConsoleVariable::getString() const
{
	return cvar->string;
}

float ConsoleVariable::getFloat() const
{
	return cvar->value;
}

int ConsoleVariable::getInt() const
{
	return cvar->integer;
}

bool ConsoleVariable::isModified() const
{
	return cvar->modified != qfalse;
}

void ConsoleVariable::setDescription(const char *description)
{
	ri.Cvar_SetDescription(cvar, description);
}

/*
** glconfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/
typedef enum {
	TC_NONE,
	TC_S3TC,  // this is for the GL_S3_s3tc extension.
	TC_S3TC_ARB  // this is for the GL_EXT_texture_compression_s3tc extension.
} textureCompression_t;

typedef enum {
	GLDRV_ICD,					// driver is integrated with window system
								// WARNING: there are tests that check for
								// > GLDRV_ICD for minidriverness, so this
								// should always be the lowest value in this
								// enum set
	GLDRV_STANDALONE,			// driver is a non-3Dfx standalone driver
	GLDRV_VOODOO				// driver is a 3Dfx standalone driver
} glDriverType_t;

typedef enum {
	GLHW_GENERIC,			// where everthing works the way it should
	GLHW_3DFX_2D3D,			// Voodoo Banshee or Voodoo3, relevant since if this is
							// the hardware type then there can NOT exist a secondary
							// display adapter
	GLHW_RIVA128,			// where you can't interpolate alpha
	GLHW_RAGEPRO,			// where you can't modulate alpha on alpha textures
	GLHW_PERMEDIA2			// where you don't have src*dst
} glHardwareType_t;

typedef struct {
	char					renderer_string[MAX_STRING_CHARS];
	char					vendor_string[MAX_STRING_CHARS];
	char					version_string[MAX_STRING_CHARS];
	char					extensions_string[BIG_INFO_STRING];

	int						maxTextureSize;			// queried from GL
	int						numTextureUnits;		// multitexture ability

	int						colorBits, depthBits, stencilBits;

	glDriverType_t			driverType;
	glHardwareType_t		hardwareType;

	qboolean				deviceSupportsGamma;
	textureCompression_t	textureCompression;
	qboolean				textureEnvAddAvailable;

	int						vidWidth, vidHeight;
	// aspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9
	float					windowAspect;

	int						displayFrequency;

	// synonymous with "does rendering consume the entire screen?", therefore
	// a Voodoo or Voodoo2 will have this set to TRUE, as will a Win32 ICD that
	// used CDS.
	qboolean				isFullscreen;
	qboolean				stereoEnabled;
	qboolean				smpActive;		// UNUSED, present for compatibility
} glconfig_t;

typedef struct {
	vec3_t		origin;
	vec3_t		axis[3];
} orientation_t;

typedef enum {
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_BEAM,
	RT_RAIL_CORE,
	RT_RAIL_RINGS,
	RT_LIGHTNING,
	RT_PORTALSURFACE,		// doesn't draw anything, just info for portals

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

	// renderfx flags
#define	RF_MINLIGHT		0x0001		// allways have some light (viewmodel, some items)
#define	RF_THIRD_PERSON		0x0002		// don't draw through eyes, only mirrors (player bodies, chat sprites)
#define	RF_FIRST_PERSON		0x0004		// only draw through eyes (view weapon, damage blood blob)
#define	RF_DEPTHHACK		0x0008		// for view weapon Z crunching

#define RF_CROSSHAIR		0x0010		// This item is a cross hair and will draw over everything similar to
	// DEPTHHACK in stereo rendering mode, with the difference that the
	// projection matrix won't be hacked to reduce the stereo separation as
	// is done for the gun.

#define	RF_NOSHADOW		0x0040		// don't add stencil shadows

#define RF_LIGHTING_ORIGIN	0x0080		// use refEntity->lightingOrigin instead of refEntity->origin
	// for lighting.  This allows entities to sink into the floor
	// with their origin going solid, and allows all parts of a
	// player to get the same lighting

#define	RF_SHADOW_PLANE		0x0100		// use refEntity->shadowPlane
#define	RF_WRAP_FRAMES		0x0200		// mod the model frames by the maxframes to allow continuous
	// animation without needing to know the frame count

typedef struct {
	refEntityType_t	reType;
	int			renderfx;

	qhandle_t	hModel;				// opaque type outside refresh

									// most recent data
	vec3_t		lightingOrigin;		// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float		shadowPlane;		// projection shadows go here, stencils go slightly lower

	vec3_t		axis[3];			// rotation vectors
	qboolean	nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float		origin[3];			// also used as MODEL_BEAM's "from"
	int			frame;				// also used as MODEL_BEAM's diameter

									// previous data for frame interpolation
	float		oldorigin[3];		// also used as MODEL_BEAM's "to"
	int			oldframe;
	float		backlerp;			// 0.0 = current, 1.0 = old

									// texturing
	int			skinNum;			// inline skin index
	qhandle_t	customSkin;			// NULL for default skin
	qhandle_t	customShader;		// use one image for the entire thing

									// misc
	byte		shaderRGBA[4];		// colors used by rgbgen entity shaders
	float		shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers
	float		shaderTime;			// subtracted from refdef time to control effect start times

									// extra sprite information
	float		radius;
	float		rotation;
} refEntity_t;

// refdef flags
#define RDF_NOWORLDMODEL	0x0001		// used for player configuration screen
#define RDF_HYPERSPACE		0x0004		// teleportation effect

#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

									// time in milliseconds for shader effects and other time dependent rendering issues
	int			time;

	int			rdflags;			// RDF_NOWORLDMODEL, etc

									// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];

	// text messages for deform text shaders
	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
} refdef_t;

typedef enum {
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;

#define	REF_API_VERSION		8

//
// these are the functions exported by the refresh module
//
typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void(*Shutdown)(qboolean destroyWindow);

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void(*BeginRegistration)(glconfig_t *config);
	qhandle_t(*RegisterModel)(const char *name);
	qhandle_t(*RegisterSkin)(const char *name);
	qhandle_t(*RegisterShader)(const char *name);
	qhandle_t(*RegisterShaderNoMip)(const char *name);
	void(*LoadWorld)(const char *name);

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void(*SetWorldVisData)(const byte *vis);

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void(*EndRegistration)(void);

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void(*ClearScene)(void);
	void(*AddRefEntityToScene)(const refEntity_t *re);
	void(*AddPolyToScene)(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num);
	int(*LightForPoint)(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
	void(*AddLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
	void(*AddAdditiveLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
	void(*RenderScene)(const refdef_t *fd);

	void(*SetColor)(const float *rgba);	// NULL = 1,1,1,1
	void(*DrawStretchPic) (float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader);	// 0 = white

																	// Draw images for cinematic rendering, pass as 32 bit rgba
	void(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
	void(*UploadCinematic) (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);

	void(*BeginFrame)(stereoFrame_t stereoFrame);

	// if the pointers are not NULL, timing info will be returned
	void(*EndFrame)(int *frontEndMsec, int *backEndMsec);


	int(*MarkFragments)(int numPoints, const vec3_t *points, const vec3_t projection,
		int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);

	int(*LerpTag)(orientation_t *tag, qhandle_t model, int startFrame, int endFrame,
		float frac, const char *tagName);
	void(*ModelBounds)(qhandle_t model, vec3_t mins, vec3_t maxs);

#ifdef __USEA3D
	void(*A3D_RenderGeometry) (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif
	void(*RegisterFont)(const char *fontName, int pointSize, fontInfo_t *font);
	void(*RemapShader)(const char *oldShader, const char *newShader, const char *offsetTime);
	qboolean(*GetEntityToken)(char *buffer, int size);
	qboolean(*inPVS)(const vec3_t p1, const vec3_t p2);

	void(*TakeVideoFrame)(int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg);
} refexport_t;

namespace interface
{
	ConsoleVariable GetConsoleVariable(const char *name, const char *value, int flags)
	{
		int translatedFlags = 0;

		if (flags & ConsoleVariableFlags::Archive)
			translatedFlags |= CVAR_ARCHIVE;
		if (flags & ConsoleVariableFlags::Cheat)
			translatedFlags |= CVAR_CHEAT;
		if (flags & ConsoleVariableFlags::Latch)
			translatedFlags |= CVAR_LATCH;

		ConsoleVariable cvar;
		cvar.cvar = ri.Cvar_Get(name, value, translatedFlags);
		return cvar;
	}
}

static void RE_Shutdown(qboolean destroyWindow)
{
	main::Shutdown(destroyWindow != qfalse);
}

static void RE_BeginRegistration(glconfig_t *config)
{
	main::Initialize();
	const bgfx::Caps *caps = bgfx::getCaps();
	config->maxTextureSize = caps->maxTextureSize;
	config->deviceSupportsGamma = qtrue;
	config->vidWidth = window::GetWidth();
	config->vidHeight = window::GetHeight();
	config->windowAspect = window::GetAspectRatio();
	config->displayFrequency = window::GetRefreshRate();
	config->isFullscreen = window::IsFullscreen() ? qtrue : qfalse;
}

static qhandle_t RE_RegisterModel(const char *name)
{
	Model *m = g_modelCache->findModel(name);

	if (!m)
		return 0;

	return (qhandle_t)m->getIndex();
}

static qhandle_t RE_RegisterSkin(const char *name)
{
	Skin *s = g_materialCache->findSkin(name);

	if (!s)
		return 0;

	return s->getHandle();
}

static qhandle_t RE_RegisterShader(const char *name)
{
	Material *m = g_materialCache->findMaterial(name);

	if (m->defaultShader)
		return 0;

	return m->index;
}

static qhandle_t RE_RegisterShaderNoMip(const char *name)
{
	Material *m = g_materialCache->findMaterial(name, MaterialLightmapId::StretchPic, false);

	if (m->defaultShader)
		return 0;

	return m->index;
}

static void RE_LoadWorld(const char *name)
{
	main::LoadWorld(name);
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
	Entity entity;
	entity.flags = 0;

	if (re->renderfx & RF_DEPTHHACK)
		entity.flags |= EntityFlags::DepthHack;
	if (re->renderfx & RF_FIRST_PERSON)
		entity.flags |= EntityFlags::FirstPerson;
	if (re->renderfx & RF_THIRD_PERSON)
		entity.flags |= EntityFlags::ThirdPerson;
	if (re->renderfx & RF_LIGHTING_ORIGIN)
		entity.flags |= EntityFlags::LightingPosition;

	if (re->reType == RT_MODEL)
		entity.type = EntityType::Model;
	else if (re->reType == RT_POLY)
		entity.type = EntityType::Poly;
	else if (re->reType == RT_SPRITE)
		entity.type = EntityType::Sprite;
	else if (re->reType == RT_BEAM)
		entity.type = EntityType::Beam;
	else if (re->reType == RT_RAIL_CORE)
		entity.type = EntityType::RailCore;
	else if (re->reType == RT_RAIL_RINGS)
		entity.type = EntityType::RailRings;
	else if (re->reType == RT_LIGHTNING)
		entity.type = EntityType::Lightning;
	else if (re->reType == RT_PORTALSURFACE)
		entity.type = EntityType::Portal;

	entity.handle = re->hModel;
	entity.lightingPosition = re->lightingOrigin;
	entity.rotation = re->axis;
	entity.nonNormalizedAxes = re->nonNormalizedAxes != qfalse;
	entity.position = re->origin;
	entity.frame = re->frame;
	entity.oldPosition = re->oldorigin;
	entity.oldFrame = re->oldframe;
	entity.lerp = 1.0f - re->backlerp;
	entity.skinNum = re->skinNum;
	entity.customSkin = re->customSkin;
	entity.customMaterial = re->customShader;
	entity.materialColor = vec4(re->shaderRGBA[0] / 255.0f, re->shaderRGBA[1] / 255.0f, re->shaderRGBA[2] / 255.0f, re->shaderRGBA[3] / 255.0f);
	entity.materialTexCoord = re->shaderTexCoord;
	entity.materialTime = re->shaderTime;
	entity.radius = re->radius;
	entity.angle = re->rotation;
	main::AddEntityToScene(entity);
}

static void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num)
{
	main::AddPolyToScene(hShader, numVerts, verts, num);
}

static int RE_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir)
{
	return main::SampleLight(point, (vec3 *)ambientLight, (vec3 *)directedLight, (vec3 *)lightDir) ? qtrue : qfalse;
}

static void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
	DynamicLight light;
	light.position_type = vec4(org, DynamicLight::Point);
	light.color_radius = vec4(r, g, b, intensity);
	main::AddDynamicLightToScene(light);
}

static void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
}

static void RE_RenderScene(const refdef_t *fd)
{
	SceneDefinition scene;
	memcpy(scene.areaMask, fd->areamask, sizeof(scene.areaMask));
	scene.flags = 0;

	if (fd->rdflags & RDF_HYPERSPACE)
		scene.flags |= SceneDefinitionFlags::Hyperspace;
	if ((fd->rdflags & RDF_NOWORLDMODEL) == 0)
		scene.flags |= SceneDefinitionFlags::World;

	scene.fov = vec2(fd->fov_x, fd->fov_y);
	scene.position = fd->vieworg;
	scene.rect = Rect(fd->x, fd->y, fd->width, fd->height);
	scene.rotation = fd->viewaxis;
	scene.time = fd->time;
	main::RenderScene(scene);
}

static void RE_SetColor(const float *rgba)
{
	main::SetColor(rgba ? vec4(rgba) : vec4::white);
}

static void RE_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader)
{
	main::DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
}

static void RE_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	main::DrawStretchRaw(x, y, w, h, cols, rows, data, client, dirty == qtrue);
}

static void RE_UploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	main::UploadCinematic(w, h, cols, rows, data, client, dirty == qtrue);
}

static void RE_BeginFrame(stereoFrame_t stereoFrame)
{
}

static void RE_EndFrame(int *frontEndMsec, int *backEndMsec)
{
	main::EndFrame();
}

static int RE_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)
{
	if (world::IsLoaded())
		return world::MarkFragments(numPoints, (const vec3 *)points, projection, maxPoints, (vec3 *)pointBuffer, maxFragments, fragmentBuffer);

	return 0;
}

static int RE_LerpTag(orientation_t *orientation, qhandle_t handle, int startFrame, int endFrame, float frac, const char *tagName)
{
	Model *m = g_modelCache->getModel(handle);

	if (m)
	{
		Transform lerped = m->lerpTag(tagName, startFrame, endFrame, frac);
		memcpy(orientation->origin, &lerped.position.x, sizeof(vec3_t));
		memcpy(orientation->axis[0], &lerped.rotation[0].x, sizeof(vec3_t));
		memcpy(orientation->axis[1], &lerped.rotation[1].x, sizeof(vec3_t));
		memcpy(orientation->axis[2], &lerped.rotation[2].x, sizeof(vec3_t));
		return qtrue;
	}
	
	return qfalse;
}

static void RE_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs)
{
	Model *m = g_modelCache->getModel(handle);

	if (m)
	{
		const Bounds bounds = m->getBounds();
		memcpy(mins, &bounds.min.x, sizeof(vec3_t));
		memcpy(maxs, &bounds.max.x, sizeof(vec3_t));
	}
}

static void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	main::RegisterFont(fontName, pointSize, font);
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

static qboolean RE_inPVS(const vec3_t p1, const vec3_t p2)
{
	if (world::IsLoaded() && world::InPvs(p1, p2))
		return qtrue;

	return qfalse;
}

static void RE_TakeVideoFrame(int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg)
{
}

// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
extern "C" Q_EXPORT refexport_t * QDECL GetRefAPI(int apiVersion, refimport_t *rimp)
{
	ri = *rimp;

	static refexport_t re;
	memset(&re, 0, sizeof(re));

	if (apiVersion != REF_API_VERSION)
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
