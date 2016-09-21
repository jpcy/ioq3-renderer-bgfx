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

#if defined(ENGINE_IOQ3)

namespace renderer {

struct cvar_t;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef unsigned char byte;
typedef int qhandle_t;

#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(__WIN64__)
#define PATH_SEP '\\'
#define QDECL __cdecl // for windows fastcall option
#else
#define PATH_SEP '/'
#define QDECL
#endif

//Ignore __attribute__ on non-gcc platforms
#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

#define LittleFloat(x) (x)
#define LittleLong(x) (x)
#define LittleShort(x) (x)
#define	LL(x) x = LittleLong(x)

#define	MAX_TOKEN_CHARS		1024	// max length of an individual token

#define	MAX_QPATH			64		// max length of a quake game pathname
#ifdef PATH_MAX
#define MAX_OSPATH			PATH_MAX
#else
#define	MAX_OSPATH			256		// max length of a filesystem pathname
#endif

struct ConsoleVariableFlags
{
	enum
	{
		Archive = 1<<0,
		Cheat   = 1<<1,
		Latch   = 1<<2
	};
};

struct ConsoleVariable
{
	void checkRange(float minValue, float maxValue, bool shouldBeIntegral);
	void clearModified();
	bool getBool() const;
	const char *getString() const;
	float getFloat() const;
	int getInt() const;
	bool isModified() const;
	void setDescription(const char *description);
	cvar_t *cvar;
};

// font support 

#define GLYPH_START 0
#define GLYPH_END 255
#define GLYPH_CHARSTART 32
#define GLYPH_CHAREND 127
#define GLYPHS_PER_FONT GLYPH_END - GLYPH_START + 1
typedef struct {
	int height;       // number of scan lines
	int top;          // top of glyph in buffer
	int bottom;       // bottom of glyph in buffer
	int pitch;        // width for copying
	int xSkip;        // x adjustment
	int imageWidth;   // width of actual image
	int imageHeight;  // height of actual image
	float s;          // x offset in image where glyph starts
	float t;          // y offset in image where glyph starts
	float s2;
	float t2;
	qhandle_t glyph;  // handle to the shader with the glyph
	char shaderName[32];
} glyphInfo_t;

typedef struct {
	glyphInfo_t glyphs[GLYPHS_PER_FONT];
	float glyphScale;
	char name[MAX_QPATH];
} fontInfo_t;

#define	MAX_MAP_AREA_BYTES		32		// bit vector of area visibility

#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_FOG			64

#define CONTENTS_NOTTEAM1		0x0080
#define CONTENTS_NOTTEAM2		0x0100
#define CONTENTS_NOBOTCLIP		0x0200

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000
	//bot specific contents types
#define	CONTENTS_TELEPORTER		0x40000
#define	CONTENTS_JUMPPAD		0x80000
#define CONTENTS_CLUSTERPORTAL	0x100000
#define CONTENTS_DONOTENTER		0x200000
#define CONTENTS_BOTCLIP		0x400000
#define CONTENTS_MOVER			0x800000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_BODY			0x2000000	// should never be on a brush, only in game
#define	CONTENTS_CORPSE			0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes not used for the bsp
#define	CONTENTS_STRUCTURAL		0x10000000	// brushes used for the bsp
#define	CONTENTS_TRANSLUCENT	0x20000000	// don't consume surface fragments inside
#define	CONTENTS_TRIGGER		0x40000000
#define	CONTENTS_NODROP			0x80000000	// don't leave bodies or items (death fog, lava)

#define	SURF_NODAMAGE			0x1		// never give falling damage
#define	SURF_SLICK				0x2		// effects game physics
#define	SURF_SKY				0x4		// lighting from environment map
#define	SURF_LADDER				0x8
#define	SURF_NOIMPACT			0x10	// don't make missile explosions
#define	SURF_NOMARKS			0x20	// don't leave missile marks
#define	SURF_FLESH				0x40	// make flesh sounds and effects
#define	SURF_NODRAW				0x80	// don't generate a drawsurface at all
#define	SURF_HINT				0x100	// make a primary bsp splitter
#define	SURF_SKIP				0x200	// completely ignore, allowing non-closed brushes
#define	SURF_NOLIGHTMAP			0x400	// surface doesn't need a lightmap
#define	SURF_POINTLIGHT			0x800	// generate lighting info at vertexes
#define	SURF_METALSTEPS			0x1000	// clanking footsteps
#define	SURF_NOSTEPS			0x2000	// no footstep sounds
#define	SURF_NONSOLID			0x4000	// don't collide against curves with this set
#define	SURF_LIGHTFILTER		0x8000	// act as a light filter during q3map -light
#define	SURF_ALPHASHADOW		0x10000	// do per-pixel light shadow casting in q3map
#define	SURF_NODLIGHT			0x20000	// don't dlight even if solid (solid lava, skies)
#define SURF_DUST				0x40000 // leave a dust trail when walking on this surface

// markfragments are returned by R_MarkFragments()
typedef struct {
	int		firstPoint;
	int		numPoints;
} markFragment_t;

typedef struct {
	vec3_t		xyz;
	float		st[2];
	byte		modulate[4];
} polyVert_t;

#define CLIENT_WINDOW_TITLE		"ioquake3"

namespace interface
{
	void CIN_UploadCinematic(int handle);
	int CIN_PlayCinematic(const char *arg0, int xpos, int ypos, int width, int height);
	void CIN_RunCinematic(int handle);
	void Cmd_Add(const char *name, void(*cmd)(void));
	void Cmd_Remove(const char *name);
	int Cmd_Argc();
	const char *Cmd_Argv(int i);
	const uint8_t *CM_ClusterPVS(int cluster);
	ConsoleVariable Cvar_Get(const char *name, const char *value, int flags);
	int Cvar_GetInteger(const char *name);
	void Cvar_Set(const char *name, const char *value);
	void Error(const char *format, ...) __attribute__((format(printf, 1, 2)));
	void FatalError(const char *format, ...) __attribute__((format(printf, 1, 2)));
	long FS_ReadFile(const char *name, uint8_t **buf);
	void FS_FreeReadFile(uint8_t *buf);
	bool FS_FileExists(const char *filename);
	char **FS_ListFiles(const char *name, const char *extension, int *numFilesFound);
	void FS_FreeListFiles(char **fileList);
	void FS_WriteFile(const char *filename, const uint8_t *buffer, size_t size);
	int GetTime();
	void *Hunk_Alloc(int size);
	void IN_Init(void *windowData);
	void IN_Shutdown();
	void Printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	void PrintDeveloperf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	void PrintWarningf(const char *format, ...) __attribute__((format(printf, 1, 2)));
}

} // namespace renderer

#endif // ENGINE_IOQ3
