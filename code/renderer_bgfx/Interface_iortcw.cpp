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
#include "Precompiled.h"
#pragma hdrstop

#if defined(ENGINE_IORTCW)

namespace renderer {

typedef enum { qfalse, qtrue }	qboolean;

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
	qboolean modified;              // set each time the cvar is changed
	int				modificationCount;	// incremented each time the cvar is changed
	float			value;				// atof( string )
	int				integer;			// atoi( string )
	qboolean	validate;
	qboolean	integral;
	float			min;
	float			max;

	cvar_t *next;
	cvar_t *prev;
	cvar_t *hashNext;
	cvar_t *hashPrev;
	int			hashIndex;
};

// cinematic states
typedef enum {
	FMV_IDLE,
	FMV_PLAY,		// play
	FMV_EOF,		// all other conditions, i.e. stop/EOF/abort
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

#define CIN_system      0x01
#define CIN_loop        0x02
#define CIN_hold        0x04
#define CIN_silent      0x08
#define CIN_shader      0x10
#define CIN_letterBox   0x20

/*
** glconfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/
typedef enum {
	TC_NONE,
	TC_S3TC,	// this is for the GL_S3_s3tc extension.
	TC_S3TC_ARB,	// this is for the GL_EXT_texture_compression_s3tc extension.
	TC_EXT_COMP_S3TC
} textureCompression_t;

typedef enum {
	GLDRV_ICD,                  // driver is integrated with window system
								// WARNING: there are tests that check for
								// > GLDRV_ICD for minidriverness, so this
								// should always be the lowest value in this
								// enum set
	GLDRV_STANDALONE,           // driver is a non-3Dfx standalone driver
	GLDRV_VOODOO                // driver is a 3Dfx standalone driver
} glDriverType_t;

typedef enum {
	GLHW_GENERIC,           // where everthing works the way it should
	GLHW_3DFX_2D3D,         // Voodoo Banshee or Voodoo3, relevant since if this is
							// the hardware type then there can NOT exist a secondary
							// display adapter
	GLHW_RIVA128,           // where you can't interpolate alpha
	GLHW_RAGEPRO,           // where you can't modulate alpha on alpha textures
	GLHW_PERMEDIA2          // where you don't have src*dst
} glHardwareType_t;

typedef struct {
	char renderer_string[MAX_STRING_CHARS];
	char vendor_string[MAX_STRING_CHARS];
	char version_string[MAX_STRING_CHARS];
	char extensions_string[4 * MAX_STRING_CHARS];                       // this is actually too short for many current cards/drivers  // (SA) doubled from 2x to 4x MAX_STRING_CHARS

	int maxTextureSize;                             // queried from GL
	int numTextureUnits;				// multitexture ability

	int colorBits, depthBits, stencilBits;

	glDriverType_t driverType;
	glHardwareType_t hardwareType;

	qboolean deviceSupportsGamma;
	textureCompression_t textureCompression;
	qboolean textureEnvAddAvailable;
	qboolean anisotropicAvailable;                  //----(SA)	added
	float maxAnisotropy;                            //----(SA)	added

													// vendor-specific support
													// NVidia
	qboolean NVFogAvailable;                    //----(SA)	added
	int NVFogMode;                                  //----(SA)	added
													// ATI
	int ATIMaxTruformTess;                          // for truform support
	int ATINormalMode;                          // for truform support
	int ATIPointMode;                           // for truform support

	int vidWidth, vidHeight;
	// aspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9
	float windowAspect;

	int displayFrequency;

	// synonymous with "does rendering consume the entire screen?", therefore
	// a Voodoo or Voodoo2 will have this set to TRUE, as will a Win32 ICD that
	// used CDS.
	qboolean isFullscreen;
	qboolean stereoEnabled;
	qboolean smpActive;                     // UNUSED, present for compatibility

	qboolean textureFilterAnisotropicAvailable;                 //DAJ
} glconfig_t;

typedef struct {
	vec3_t		origin;
	vec3_t		axis[3];
} orientation_t;

typedef enum {
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_SPLASH,  // ripple effect
	RT_BEAM,
	RT_RAIL_CORE,
	RT_RAIL_CORE_TAPER, // a modified core that creates a properly texture mapped core that's wider at one end
	RT_RAIL_RINGS,
	RT_LIGHTNING,
	RT_PORTALSURFACE,       // doesn't draw anything, just info for portals

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

// renderfx flags
#define RF_MINLIGHT         0x0001       // allways have some light (viewmodel, some items)
#define RF_THIRD_PERSON     0x0002       // don't draw through eyes, only mirrors (player bodies, chat sprites)
#define RF_FIRST_PERSON     0x0004       // only draw through eyes (view weapon, damage blood blob)
#define RF_DEPTHHACK        0x0008       // for view weapon Z crunching

#define RF_CROSSHAIR		0x0010		// This item is a cross hair and will draw over everything similar to
// DEPTHHACK in stereo rendering mode, with the difference that the
// projection matrix won't be hacked to reduce the stereo separation as
// is done for the gun.

#define RF_NOSHADOW         0x0040      // don't add stencil shadows

#define RF_LIGHTING_ORIGIN  0x0080     // use refEntity->lightingOrigin instead of refEntity->origin
// for lighting.  This allows entities to sink into the floor
// with their origin going solid, and allows all parts of a
// player to get the same lighting
#define RF_SHADOW_PLANE     0x0100     // use refEntity->shadowPlane
#define RF_WRAP_FRAMES      0x0200     // mod the model frames by the maxframes to allow continuous
// animation without needing to know the frame count

#define RF_HILIGHT          ( 1 << 8 )  // more than RF_MINLIGHT.  For when an object is "Highlighted" (looked at/training identification/etc)
#define RF_BLINK            ( 1 << 9 )  // eyes in 'blink' state

typedef struct {
	refEntityType_t reType;
	int renderfx;

	qhandle_t hModel;               // opaque type outside refresh

									// most recent data
	vec3_t lightingOrigin;          // so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float shadowPlane;              // projection shadows go here, stencils go slightly lower

	vec3_t axis[3];                 // rotation vectors
	vec3_t torsoAxis[3];            // rotation vectors for torso section of skeletal animation
	qboolean nonNormalizedAxes;     // axis are not normalized, i.e. they have scale
	float origin[3];                // also used as MODEL_BEAM's "from"
	int frame;                      // also used as MODEL_BEAM's diameter
	int torsoFrame;                 // skeletal torso can have frame independant of legs frame

	vec3_t scale;       //----(SA)	added

						// previous data for frame interpolation
	float oldorigin[3];             // also used as MODEL_BEAM's "to"
	int oldframe;
	int oldTorsoFrame;
	float backlerp;                 // 0.0 = current, 1.0 = old
	float torsoBacklerp;

	// texturing
	int skinNum;                    // inline skin index
	qhandle_t customSkin;           // NULL for default skin
	qhandle_t customShader;         // use one image for the entire thing

									// misc
	byte shaderRGBA[4];             // colors used by rgbgen entity shaders
	float shaderTexCoord[2];        // texture coordinates used by tcMod entity modifiers
	float shaderTime;               // subtracted from refdef time to control effect start times

									// extra sprite information
	float radius;
	float rotation;

	// Ridah
	vec3_t fireRiseDir;

	// Ridah, entity fading (gibs, debris, etc)
	int fadeStartTime, fadeEndTime;

	float hilightIntensity;         //----(SA)	added

	int reFlags;

	int entityNum;                  // currentState.number, so we can attach rendering effects to specific entities (Zombie)

} refEntity_t;

// refdef flags
#define RDF_NOWORLDMODEL    0x0001       // used for player configuration screen
#define RDF_HYPERSPACE      0x0004       // teleportation effect

// Rafael
#define RDF_SKYBOXPORTAL    0x0008

#define RDF_DRAWSKYBOX      0x0010      // the above marks a scene as being a 'portal sky'.  this flag says to draw it or not

//----(SA)
#define RDF_UNDERWATER      ( 1 << 4 )  // so the renderer knows to use underwater fog when the player is underwater
#define RDF_DRAWINGSKY      ( 1 << 5 )
#define RDF_SNOOPERVIEW     ( 1 << 6 )  //----(SA)	added

//----(SA)

//                                                                  //
// WARNING:: synch FOG_SERVER in sv_ccmds.c if you change anything	//
//                                                                  //
typedef enum {
	FOG_NONE,       //	0

	FOG_SKY,        //	1	fog values to apply to the sky when using density fog for the world (non-distance clipping fog) (only used if(glfogsettings[FOG_MAP].registered) or if(glfogsettings[FOG_MAP].registered))
	FOG_PORTALVIEW, //	2	used by the portal sky scene
	FOG_HUD,        //	3	used by the 3D hud scene

					//		The result of these for a given frame is copied to the scene.glFog when the scene is rendered

					// the following are fogs applied to the main world scene
	FOG_MAP,        //	4	use fog parameter specified using the "fogvars" in the sky shader
	FOG_WATER,      //	5	used when underwater
	FOG_SERVER,     //	6	the server has set my fog (probably a target_fog) (keep synch in sv_ccmds.c !!!)
	FOG_CURRENT,    //	7	stores the current values when a transition starts
	FOG_LAST,       //	8	stores the current values when a transition starts
	FOG_TARGET,     //	9	the values it's transitioning to.

	FOG_CMD_SWITCHFOG,  // 10	transition to the fog specified in the second parameter of R_SetFog(...) (keep synch in sv_ccmds.c !!!)

	NUM_FOGS
} glfogType_t;


typedef struct {
	int mode;                   // GL_LINEAR, GL_EXP
	int hint;                   // GL_DONT_CARE
	int startTime;              // in ms
	int finishTime;             // in ms
	float color[4];
	float start;                // near
	float end;                  // far
	qboolean useEndForClip;     // use the 'far' value for the far clipping plane
	float density;              // 0.0-1.0
	qboolean registered;        // has this fog been set up?
	qboolean drawsky;           // draw skybox
	qboolean clearscreen;       // clear the GL color buffer

	int dirty;
} glfog_t;

//----(SA)	end


#define MAX_RENDER_STRINGS          8
#define MAX_RENDER_STRING_LENGTH    32

typedef struct {
	int x, y, width, height;
	float fov_x, fov_y;
	vec3_t vieworg;
	vec3_t viewaxis[3];             // transformation matrix

	int time;           // time in milliseconds for shader effects and other time dependent rendering issues
	int rdflags;                    // RDF_NOWORLDMODEL, etc

									// 1 bits will prevent the associated area from rendering at all
	byte areamask[MAX_MAP_AREA_BYTES];




	// text messages for deform text shaders
	char text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];


	//----(SA)	added (needed to pass fog infos into the portal sky scene)
	glfog_t glfog;
	//----(SA)	end

} refdef_t;

typedef enum {
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;

#define	REF_API_VERSION		8

typedef enum {
	h_high,
	h_low,
	h_dontcare
} ha_pref;

// print levels from renderer (FIXME: set up for game / cgame?)
typedef enum {
	PRINT_ALL,
	PRINT_DEVELOPER,		// only print when "developer 1"
	PRINT_WARNING,
	PRINT_ERROR
} printParm_t;


#ifdef ERR_FATAL
#undef ERR_FATAL			// this is be defined in malloc.h
#endif

// parameters to the main Error routine
typedef enum {
	ERR_FATAL,                  // exit the entire game with a popup window
	ERR_DROP,                   // print to console and disconnect from game
	ERR_SERVERDISCONNECT,       // don't kill server
	ERR_DISCONNECT,             // client disconnected from the server
	ERR_NEED_CD,                // pop up the need-cd dialog
	ERR_ENDGAME                 // not an error.  just clean up properly, exit to the menu, and start up the "endgame" menu  //----(SA)	added
} errorParm_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	// print message on the local console
	void	(QDECL *Printf)(int printLevel, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

	// abort the game
	void	(QDECL *Error)(int errorLevel, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	int(*Milliseconds)(void);

	// stack based memory allocation for per-level things that
	// won't be freed
	void(*Hunk_Clear)(void);
#ifdef HUNK_DEBUG
	void    *(*Hunk_AllocDebug)(int size, ha_pref pref, char *label, char *file, int line);
#else
	void    *(*Hunk_Alloc)(int size, ha_pref pref);
#endif
	void    *(*Hunk_AllocateTempMemory)(int size);
	void(*Hunk_FreeTempMemory)(void *block);

	void    *(*Z_Malloc)(int bytes);
	void(*Free)(void *buf);

	cvar_t  *(*Cvar_Get)(const char *name, const char *value, int flags);
	void(*Cvar_Set)(const char *name, const char *value);
	void(*Cvar_SetValue) (const char *name, float value);
	void(*Cvar_CheckRange)(cvar_t *cv, float minVal, float maxVal, qboolean shouldBeIntegral);

	int(*Cvar_VariableIntegerValue) (const char *var_name);

	void(*Cmd_AddCommand)(const char *name, void(*cmd) (void));
	void(*Cmd_RemoveCommand)(const char *name);

	int(*Cmd_Argc)(void);
	char    *(*Cmd_Argv)(int i);

	void(*Cmd_ExecuteText)(int exec_when, const char *text);

	byte	*(*CM_ClusterPVS)(int cluster);

	// visualization for debugging collision detection
	void(*CM_DrawDebugSurface)(void(*drawPoly) (int color, int numPoints, float *points));

	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int(*FS_FileIsInPAK)(const char *name, int *pChecksum);
	long(*FS_ReadFile)(const char *name, void **buf);
	void(*FS_FreeFile)(void *buf);
	char ** (*FS_ListFiles)(const char *name, const char *extension, int *numfilesfound);
	void(*FS_FreeFileList)(char **filelist);
	void(*FS_WriteFile)(const char *qpath, const void *buffer, int size);
	qboolean(*FS_FileExists)(const char *file);

	// cinematic stuff
	void(*CIN_UploadCinematic)(int handle);
	int(*CIN_PlayCinematic)(const char *arg0, int xpos, int ypos, int width, int height, int bits);
	e_status(*CIN_RunCinematic)(int handle);

	void(*CL_WriteAVIVideoFrame)(const byte *buffer, int size);

	// input event handling
	void(*IN_Init)(void *windowData);
	void(*IN_Shutdown)(void);
	void(*IN_Restart)(void);

	// math
	long(*ftol)(float f);

	// system stuff
	void(*Sys_SetEnv)(const char *name, const char *value);
	void(*Sys_GLimpSafeInit)(void);
	void(*Sys_GLimpInit)(void);
	qboolean(*Sys_LowPhysicalMemory)(void);
} refimport_t;

static refimport_t ri;

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
	qboolean(*GetSkinModel)(qhandle_t skinid, const char *type, char *name);    //----(SA)	added
	qhandle_t(*GetShaderFromModel)(qhandle_t modelid, int surfnum, int withlightmap);                //----(SA)	added

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
	int(*LightForPoint)(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
	void(*AddPolyToScene)(qhandle_t hShader, int numVerts, const polyVert_t *verts);
	// Ridah
	void(*AddPolysToScene)(qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys);
	// done.
	void(*AddLightToScene)(const vec3_t org, float intensity, float r, float g, float b, int overdraw);
	//----(SA)
	void(*AddCoronaToScene)(const vec3_t org, float r, float g, float b, float scale, int id, int flags);
	void(*SetFog)(int fogvar, int var1, int var2, float r, float g, float b, float density);
	//----(SA)
	void(*RenderScene)(const refdef_t *fd);

	void(*SetColor)(const float *rgba);    // NULL = 1,1,1,1
	void(*DrawStretchPic)(float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader); // 0 = white
	void(*DrawStretchPicGradient)(float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader, const float *gradientColor, int gradientType);

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void(*DrawStretchRaw)(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
	void(*UploadCinematic)(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);

	void(*BeginFrame)(stereoFrame_t stereoFrame);

	// if the pointers are not NULL, timing info will be returned
	void(*EndFrame)(int *frontEndMsec, int *backEndMsec);


	int(*MarkFragments)(int numPoints, const vec3_t *points, const vec3_t projection,
		int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);

	int(*LerpTag)(orientation_t *tag, const refEntity_t *refent, const char *tagName, int startIndex);
	void(*ModelBounds)(qhandle_t model, vec3_t mins, vec3_t maxs);

#ifdef __USEA3D
	void(*A3D_RenderGeometry)(void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif

	void(*RegisterFont)(const char *fontName, int pointSize, fontInfo_t *font);
	void(*RemapShader)(const char *oldShader, const char *newShader, const char *offsetTime);
	// RF
	void(*ZombieFXAddNewHit)(int entityNum, const vec3_t hitPos, const vec3_t hitDir);

	qboolean(*GetEntityToken)(char *buffer, int size);

	void(*TakeVideoFrame)(int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg);
} refexport_t;

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
	//ri.Cvar_SetDescription(cvar, description);
}

namespace interface
{
	void CIN_UploadCinematic(int handle)
	{
		ri.CIN_UploadCinematic(handle);
	}

	int CIN_PlayCinematic(const char *arg0, int xpos, int ypos, int width, int height)
	{
		return ri.CIN_PlayCinematic(arg0, xpos, ypos, width, height, CIN_loop | CIN_silent | CIN_shader);
	}

	void CIN_RunCinematic(int handle)
	{
		ri.CIN_RunCinematic(handle);
	}

	void Cmd_Add(const char *name, void(*cmd)(void))
	{
		ri.Cmd_AddCommand(name, cmd);
	}

	void Cmd_Remove(const char *name)
	{
		ri.Cmd_RemoveCommand(name);
	}

	int Cmd_Argc()
	{
		return ri.Cmd_Argc();
	}

	const char *Cmd_Argv(int i)
	{
		return ri.Cmd_Argv(i);
	}

	const uint8_t *CM_ClusterPVS(int cluster)
	{
		return ri.CM_ClusterPVS(cluster);
	}

	ConsoleVariable Cvar_Get(const char *name, const char *value, int flags)
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

	int Cvar_GetInteger(const char *name)
	{
		return ri.Cvar_VariableIntegerValue(name);
	}

	void Cvar_Set(const char *name, const char *value)
	{
		ri.Cvar_Set(name, value);
	}

	static void Error(int errorLevel, const char *format, va_list args)
	{
		char text[4096];
		util::Vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		ri.Error(errorLevel, "%s", text);
	}

	void Error(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		Error(ERR_DROP, format, args);
	}

	void FatalError(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		Error(ERR_FATAL, format, args);
	}

	long FS_ReadFile(const char *name, uint8_t **buf)
	{
		return ri.FS_ReadFile(name, (void **)buf);
	}

	void FS_FreeReadFile(uint8_t *buf)
	{
		ri.FS_FreeFile(buf);
	}

	bool FS_FileExists(const char *filename)
	{
		return ri.FS_FileExists(filename) != qfalse;
	}

	char **FS_ListFiles(const char *name, const char *extension, int *numFilesFound)
	{
		return ri.FS_ListFiles(name, extension, numFilesFound);
	}

	void FS_FreeListFiles(char **fileList)
	{
		ri.FS_FreeFileList(fileList);
	}

	void FS_WriteFile(const char *filename, const uint8_t *buffer, size_t size)
	{
		ri.FS_WriteFile(filename, buffer, (int)size);
	}

	int GetTime()
	{
		return ri.Milliseconds();
	}

	void *Hunk_Alloc(int size)
	{
		return ri.Hunk_Alloc(size, h_low);
	}

	void IN_Init(void *windowData)
	{
		ri.IN_Init(windowData);
	}

	void IN_Shutdown()
	{
		ri.IN_Shutdown();
	}

	static void Print(int printLevel, const char *format, va_list args)
	{
		char text[4096];
		util::Vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		ri.Printf(printLevel, "%s", text);
	}

	void Printf(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		Print(PRINT_ALL, format, args);
	}

	void PrintDeveloperf(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		Print(PRINT_DEVELOPER, format, args);
	}

	void PrintWarningf(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		Print(PRINT_WARNING, format, args);
	}
}

static Entity ConvertEntity(const refEntity_t *re)
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
	entity.torsoRotation = re->torsoAxis;
	entity.nonNormalizedAxes = re->nonNormalizedAxes != qfalse;
	entity.position = re->origin;
	entity.frame = re->frame;
	entity.torsoFrame = re->torsoFrame;
	entity.oldPosition = re->oldorigin;
	entity.oldFrame = re->oldframe;
	entity.oldTorsoFrame = re->oldTorsoFrame;
	entity.lerp = 1.0f - re->backlerp;
	entity.torsoLerp = 1.0f - re->torsoBacklerp;
	entity.skinNum = re->skinNum;
	entity.customSkin = re->customSkin;
	entity.customMaterial = re->customShader;
	entity.materialColor = vec4(re->shaderRGBA[0] / 255.0f, re->shaderRGBA[1] / 255.0f, re->shaderRGBA[2] / 255.0f, re->shaderRGBA[3] / 255.0f);
	entity.materialTexCoord = re->shaderTexCoord;
	entity.materialTime = re->shaderTime;
	entity.radius = re->radius;
	entity.angle = re->rotation;
	return entity;
}

static void RE_Shutdown(qboolean destroyWindow)
{
	main::Shutdown(destroyWindow != qfalse);
}

static void RE_BeginRegistration(glconfig_t *config)
{
	main::Initialize();
	const bgfx::Caps *caps = bgfx::getCaps();
	config->maxTextureSize = caps->limits.maxTextureSize;
	config->deviceSupportsGamma = window::IsFullscreen() ? qtrue : qfalse;
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

static qboolean RE_GetSkinModel(qhandle_t handle, const char *type, char *name)
{
	Skin *skin = g_materialCache->getSkin(handle);

	if (!skin)
		return qfalse;

	// client is requesting scale from the skin rather than a model
	if (!util::Stricmp(type, "playerscale"))
	{
		util::Sprintf(name, MAX_QPATH, "%.2f %.2f %.2f", skin->getScale(), skin->getScale(), skin->getScale());
		return qtrue;
	}

	const char *modelName = skin->findModelName(type);

	if (modelName)
	{
		util::Strncpyz(name, modelName, MAX_QPATH);
		return qtrue;
	}

	return qfalse;
}

static qhandle_t RE_GetShaderFromModel(qhandle_t modelid, int surfnum, int withlightmap)
{
	Model *model = g_modelCache->getModel(modelid);

	if (model)
	{
		Material *mat = model->getMaterial((size_t)std::max(0, surfnum));

		if (mat)
			return mat->index;
	}

	return 0;
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
	main::AddEntityToScene(ConvertEntity(re));
}

static int RE_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir)
{
	return main::SampleLight(point, (vec3 *)ambientLight, (vec3 *)directedLight, (vec3 *)lightDir) ? qtrue : qfalse;
}

static void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts)
{
	main::AddPolyToScene(hShader, numVerts, verts, 1);
}

static void RE_AddPolysToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num)
{
	main::AddPolyToScene(hShader, numVerts, verts, num);
}

static void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b, int overdraw)
{
	DynamicLight light;
	light.position_type = vec4(org, DynamicLight::Point);
	light.color_radius = vec4(r, g, b, intensity);
	main::AddDynamicLightToScene(light);
}

static void RE_AddCoronaToScene(const vec3_t org, float r, float g, float b, float scale, int id, int flags)
{
}

static void RE_SetFog(int fogvar, int var1, int var2, float r, float g, float b, float density)
{
}

static void RE_RenderScene(const refdef_t *fd)
{
	SceneDefinition scene;
	memcpy(scene.areaMask, fd->areamask, sizeof(scene.areaMask));
	scene.flags = 0;

	if (fd->rdflags & RDF_HYPERSPACE)
		scene.flags |= SceneDefinitionFlags::Hyperspace;
	if (fd->rdflags & RDF_SKYBOXPORTAL)
		scene.flags |= SceneDefinitionFlags::SkyboxPortal;
	if (fd->rdflags & RDF_DRAWSKYBOX)
		scene.flags |= SceneDefinitionFlags::ContainsSkyboxPortal;
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

static void RE_DrawStretchPicGradient(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, const float *gradientColor, int gradientType)
{
	main::DrawStretchPicGradient(x, y, w, h, s1, t1, s2, t2, hShader, gradientColor ? vec4(gradientColor) : vec4::white);
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

static int RE_MarkFragments(int orientation, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer)
{
	if (world::IsLoaded())
		return world::MarkFragments(4, (const vec3 *)points, projection, maxPoints, (vec3 *)pointBuffer, maxFragments, fragmentBuffer);

	return 0;
}

static int RE_LerpTag(orientation_t *tag, const refEntity_t *refent, const char *tagName, int startIndex)
{
	Model *m = g_modelCache->getModel(refent->hModel);

	if (!m)
		return -1;

	Transform lerped;
	int tagIndex = m->lerpTag(tagName, ConvertEntity(refent), startIndex, &lerped);

	if (tagIndex >= 0)
	{
		memcpy(tag->origin, &lerped.position.x, sizeof(vec3_t));
		memcpy(tag->axis[0], &lerped.rotation[0].x, sizeof(vec3_t));
		memcpy(tag->axis[1], &lerped.rotation[1].x, sizeof(vec3_t));
		memcpy(tag->axis[2], &lerped.rotation[2].x, sizeof(vec3_t));
	}

	return tagIndex;
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

static void RE_ZombieFXAddNewHit(int entityNum, const vec3_t hitPos, const vec3_t hitDir)
{
}

static qboolean RE_GetEntityToken(char *buffer, int size)
{
	if (world::IsLoaded() && world::GetEntityToken(buffer, size))
		return qtrue;

	return qfalse;
}

static void RE_TakeVideoFrame(int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg)
{
}

#if (defined _MSC_VER)
#define Q_EXPORT __declspec(dllexport)
#elif (defined __SUNPRO_C)
#define Q_EXPORT __global
#elif ((__GNUC__ >= 3) && (!__EMX__) && (!sun))
#define Q_EXPORT __attribute__((visibility("default")))
#else
#define Q_EXPORT
#endif

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
		interface::Printf("Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion);
		return NULL;
	}

	re.Shutdown = RE_Shutdown;
	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.LoadWorld = RE_LoadWorld;
	re.GetSkinModel = RE_GetSkinModel;
	re.GetShaderFromModel = RE_GetShaderFromModel;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;
	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.LightForPoint = RE_LightForPoint;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.AddPolysToScene = RE_AddPolysToScene;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddCoronaToScene = RE_AddCoronaToScene;
	re.SetFog = RE_SetFog;
	re.RenderScene = RE_RenderScene;
	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_DrawStretchPic;
	re.DrawStretchPicGradient = RE_DrawStretchPicGradient;
	re.DrawStretchRaw = RE_DrawStretchRaw;
	re.UploadCinematic = RE_UploadCinematic;
	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;
	re.MarkFragments = RE_MarkFragments;
	re.LerpTag = RE_LerpTag;
	re.ModelBounds = RE_ModelBounds;
	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = RE_RemapShader;
	re.ZombieFXAddNewHit = RE_ZombieFXAddNewHit;
	re.GetEntityToken = RE_GetEntityToken;
	re.TakeVideoFrame = RE_TakeVideoFrame;

	return &re;
}

} // namespace renderer

#endif // ENGINE_IORTCW
