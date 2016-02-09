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

#define R_MODE_FALLBACK 3 // 640 * 480

namespace renderer {

static SDL_Window *SDL_window = NULL;
static float displayAspect = 0.0f;

struct VideoMode
{
	const char *description;
	int width, height;
	float pixelAspect;
};

VideoMode r_vidModes[] =
{
	{ "Mode  0: 320x240",		320,	240,	1 },
	{ "Mode  1: 400x300",		400,	300,	1 },
	{ "Mode  2: 512x384",		512,	384,	1 },
	{ "Mode  3: 640x480",		640,	480,	1 },
	{ "Mode  4: 800x600",		800,	600,	1 },
	{ "Mode  5: 960x720",		960,	720,	1 },
	{ "Mode  6: 1024x768",		1024,	768,	1 },
	{ "Mode  7: 1152x864",		1152,	864,	1 },
	{ "Mode  8: 1280x1024",		1280,	1024,	1 },
	{ "Mode  9: 1600x1200",		1600,	1200,	1 },
	{ "Mode 10: 2048x1536",		2048,	1536,	1 },
	{ "Mode 11: 856x480 (wide)",856,	480,	1 }
};

static int s_numVidModes = ARRAY_LEN(r_vidModes);

enum class SetModeResult
{
	OK,
	InvalidFullScreen,
	InvalidMode,
	Unknown
};

static bool GetModeInfo(int *width, int *height, float *windowAspect, int mode)
{
	if (mode < -1)
		return false;
	if (mode >= s_numVidModes)
		return false;

	float pixelAspect;

	if (mode == -1)
	{
		*width = g_cvars.customwidth->integer;
		*height = g_cvars.customheight->integer;
		pixelAspect = g_cvars.customPixelAspect->value;
	}
	else
	{
		auto vm = &r_vidModes[mode];
		*width  = vm->width;
		*height = vm->height;
		pixelAspect = vm->pixelAspect;
	}

	*windowAspect = *width / (*height * pixelAspect);
	return true;
}

static SetModeResult SetMode(bool gl, int mode, bool fullscreen, bool noborder)
{
	ri.Printf(PRINT_ALL, "Initializing display\n");
	Uint32 flags = SDL_WINDOW_SHOWN;

	if (gl)
		flags |= SDL_WINDOW_OPENGL;

	if (g_cvars.allowResize->integer != 0)
		flags |= SDL_WINDOW_RESIZABLE;

	SDL_Surface *icon = NULL;
	
#ifdef USE_ICON
	icon = SDL_CreateRGBSurfaceFrom(
			(void *)CLIENT_WINDOW_ICON.pixel_data,
			CLIENT_WINDOW_ICON.width,
			CLIENT_WINDOW_ICON.height,
			CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
			CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
			0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
			0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
			);
#endif

	// If a window exists, note its display index
	int display = 0;

	if (SDL_window != NULL)
		display = SDL_GetWindowDisplayIndex(SDL_window);

	SDL_DisplayMode desktopMode;

	// Get the display aspect ratio
	if (SDL_GetDesktopDisplayMode(display, &desktopMode) == 0)
	{
		displayAspect = desktopMode.w / (float)desktopMode.h;
		ri.Printf(PRINT_ALL, "Display aspect: %.3f\n", displayAspect);
	}
	else
	{
		Com_Memset(&desktopMode, 0, sizeof(SDL_DisplayMode));
		ri.Printf(PRINT_ALL, "Cannot determine display aspect, assuming 1.333\n");
	}

	ri.Printf (PRINT_ALL, "...setting mode %d:", mode );

	if (mode == -2)
	{
		// use desktop video resolution
		if( desktopMode.h > 0 )
		{
			glConfig.vidWidth = desktopMode.w;
			glConfig.vidHeight = desktopMode.h;
		}
		else
		{
			glConfig.vidWidth = 640;
			glConfig.vidHeight = 480;
			ri.Printf(PRINT_ALL, "Cannot determine display resolution, assuming 640x480\n");
		}

		glConfig.windowAspect = glConfig.vidWidth / (float)glConfig.vidHeight;
	}
	else if (!GetModeInfo(&glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode))
	{
		ri.Printf(PRINT_ALL, " invalid mode\n");
		return SetModeResult::InvalidMode;
	}

	ri.Printf(PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight);

	// Center window
	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;

	if (SDL_window != NULL)
	{
		SDL_GetWindowPosition(SDL_window, &x, &y);
		ri.Printf(PRINT_DEVELOPER, "Existing window at %dx%d before being destroyed\n", x, y);
		SDL_DestroyWindow(SDL_window);
		SDL_window = NULL;
	}

	if (fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN;
		glConfig.isFullscreen = qtrue;
	}
	else
	{
		if (noborder)
			flags |= SDL_WINDOW_BORDERLESS;

		glConfig.isFullscreen = qfalse;
	}

	if (g_cvars.centerWindow->integer && !glConfig.isFullscreen)
	{
		x = y = SDL_WINDOWPOS_CENTERED;
	}
	
	if ((SDL_window = SDL_CreateWindow(CLIENT_WINDOW_TITLE, x, y, glConfig.vidWidth, glConfig.vidHeight, flags)) == 0)
	{
		ri.Printf(PRINT_DEVELOPER, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		goto finished;
	}

	if (fullscreen)
	{
		SDL_DisplayMode mode;
		mode.format = SDL_PIXELFORMAT_RGB24;
		mode.w = glConfig.vidWidth;
		mode.h = glConfig.vidHeight;
		mode.refresh_rate = glConfig.displayFrequency = ri.Cvar_VariableIntegerValue("r_displayRefresh");
		mode.driverdata = NULL;

		if (SDL_SetWindowDisplayMode(SDL_window, &mode) < 0)
		{
			ri.Printf(PRINT_DEVELOPER, "SDL_SetWindowDisplayMode failed: %s\n", SDL_GetError());
			goto finished;
		}
	}

	SDL_SetWindowIcon(SDL_window, icon);

finished:
	SDL_FreeSurface(icon);

	if (!SDL_window)
	{
		ri.Printf(PRINT_ALL, "Couldn't get a visual\n");
		return SetModeResult::InvalidMode;
	}

	return SetModeResult::OK;
}

static bool StartDriverAndSetMode(bool gl, int mode, bool fullscreen, bool noborder)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			ri.Printf(PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError());
			return false;
		}

		auto driverName = SDL_GetCurrentVideoDriver();
		ri.Printf(PRINT_ALL, "SDL using driver \"%s\"\n", driverName);
		ri.Cvar_Set("r_sdlDriver", driverName);
	}

	if (fullscreen && ri.Cvar_VariableIntegerValue("in_nograb"))
	{
		ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set( "r_fullscreen", "0" );
		g_cvars.fullscreen->modified = qfalse;
		fullscreen = false;
	}
	
	switch (SetMode(gl, mode, fullscreen, noborder))
	{
	case SetModeResult::InvalidFullScreen:
		ri.Printf(PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n");
		return false;
	case SetModeResult::InvalidMode:
		ri.Printf(PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode);
		return false;
	default:
		break;
	}

	return true;
}

void Window_Initialize(bool gl)
{
	if (ri.Cvar_VariableIntegerValue("com_abnormalExit"))
	{
		ri.Cvar_Set("r_mode", va("%d", R_MODE_FALLBACK));
		ri.Cvar_Set("r_fullscreen", "0");
		ri.Cvar_Set("r_centerWindow", "0");
		ri.Cvar_Set("com_abnormalExit", "0");
	}

	// Create the window and set up the context
	if (StartDriverAndSetMode(gl, g_cvars.mode->integer, g_cvars.fullscreen->integer != 0, g_cvars.noborder->integer != 0))
		goto success;

	if (g_cvars.noborder->integer != 0)
	{
		// Try again with a window border
		if (StartDriverAndSetMode(gl, g_cvars.mode->integer, g_cvars.fullscreen->integer != 0, false))
			goto success;
	}

	// Finally, try the default screen resolution
	if (g_cvars.mode->integer != R_MODE_FALLBACK)
	{
		ri.Printf(PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n", g_cvars.mode->integer, R_MODE_FALLBACK);

		if (StartDriverAndSetMode(gl, R_MODE_FALLBACK, false, false))
			goto success;
	}

	// Nothing worked, give up
	ri.Error(ERR_FATAL, "Could not load OpenGL subsystem");

success:
	// This depends on SDL_INIT_VIDEO, hence having it here
	ri.IN_Init(SDL_window);

	bgfx::sdlSetWindow(SDL_window);
}

void Window_Shutdown()
{
	ri.IN_Shutdown();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

} // namespace renderer
