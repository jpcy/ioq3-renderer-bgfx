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
namespace window {

struct Window
{
	float aspectRatio;
	bool isFullscreen;
	int refreshRate;
	int width = 0, height = 0;
};

static Window s_window;
static SDL_Window *SDL_window = NULL;
static float displayAspect = 0.0f;

struct VideoMode
{
	const char *description;
	int width, height;
	float pixelAspect;
};

static VideoMode r_vidModes[] =
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

static int s_numVidModes = BX_COUNTOF(r_vidModes);

enum class SetModeResult
{
	OK,
	InvalidFullScreen,
	InvalidMode,
	Unknown
};

static bool GetModeInfo(int mode)
{
	if (mode < -1)
		return false;
	if (mode >= s_numVidModes)
		return false;

	float pixelAspect;

	if (mode == -1)
	{
		s_window.width = g_cvars.customwidth.getInt();
		s_window.height = g_cvars.customheight.getInt();
		pixelAspect = g_cvars.customPixelAspect.getFloat();
	}
	else
	{
		const VideoMode &vm = r_vidModes[mode];
		s_window.width  = vm.width;
		s_window.height = vm.height;
		pixelAspect = vm.pixelAspect;
	}

	s_window.aspectRatio = s_window.width / (s_window.height * pixelAspect);
	return true;
}

static SetModeResult SetMode(int mode, bool fullscreen, bool noborder)
{
	interface::Printf("Initializing display\n");
	Uint32 flags = SDL_WINDOW_SHOWN;

	if (g_cvars.allowResize.getBool())
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
		interface::Printf("Display aspect: %.3f\n", displayAspect);
	}
	else
	{
		memset(&desktopMode, 0, sizeof(SDL_DisplayMode));
		interface::Printf("Cannot determine display aspect, assuming 1.333\n");
	}

	interface::Printf("...setting mode %d:", mode );

	if (mode == -2)
	{
		// use desktop video resolution
		if( desktopMode.h > 0 )
		{
			s_window.width = desktopMode.w;
			s_window.height = desktopMode.h;
		}
		else
		{
			s_window.width = 640;
			s_window.height = 480;
			interface::Printf("Cannot determine display resolution, assuming 640x480\n");
		}

		s_window.aspectRatio = s_window.width / (float)s_window.height;
	}
	else if (!GetModeInfo(mode))
	{
		interface::Printf(" invalid mode\n");
		return SetModeResult::InvalidMode;
	}

	interface::Printf(" %d %d\n", s_window.width, s_window.height);

	// Center window
	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;

	if (SDL_window != NULL)
	{
		SDL_GetWindowPosition(SDL_window, &x, &y);
		interface::PrintDeveloperf("Existing window at %dx%d before being destroyed\n", x, y);
		SDL_DestroyWindow(SDL_window);
		SDL_window = NULL;
	}

	if (fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN;
		s_window.isFullscreen = true;
	}
	else
	{
		if (noborder)
			flags |= SDL_WINDOW_BORDERLESS;

		s_window.isFullscreen = false;
	}

	if (g_cvars.centerWindow.getBool() && !s_window.isFullscreen)
	{
		x = y = SDL_WINDOWPOS_CENTERED;
	}
	
	if ((SDL_window = SDL_CreateWindow(CLIENT_WINDOW_TITLE, x, y, s_window.width, s_window.height, flags)) == 0)
	{
		interface::PrintDeveloperf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		goto finished;
	}

	if (fullscreen)
	{
		SDL_DisplayMode mode;
		mode.format = SDL_PIXELFORMAT_RGB24;
		mode.w = s_window.width;
		mode.h = s_window.height;
		mode.refresh_rate = s_window.refreshRate = interface::Cvar_GetInteger("r_displayRefresh");
		mode.driverdata = NULL;

		if (SDL_SetWindowDisplayMode(SDL_window, &mode) < 0)
		{
			interface::PrintDeveloperf("SDL_SetWindowDisplayMode failed: %s\n", SDL_GetError());
			goto finished;
		}
	}

	SDL_SetWindowIcon(SDL_window, icon);

finished:
	SDL_FreeSurface(icon);

	if (!SDL_window)
	{
		interface::Printf("Couldn't get a visual\n");
		return SetModeResult::InvalidMode;
	}

	return SetModeResult::OK;
}

static bool StartDriverAndSetMode(int mode, bool fullscreen, bool noborder)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			interface::Printf("SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError());
			return false;
		}

		const char *driverName = SDL_GetCurrentVideoDriver();
		interface::Printf("SDL using driver \"%s\"\n", driverName);
		interface::Cvar_Set("r_sdlDriver", driverName);
	}

	if (fullscreen && interface::Cvar_GetInteger("in_nograb"))
	{
		interface::Printf("Fullscreen not allowed with in_nograb 1\n");
		interface::Cvar_Set( "r_fullscreen", "0" );
		g_cvars.fullscreen.clearModified();
		fullscreen = false;
	}
	
	switch (SetMode(mode, fullscreen, noborder))
	{
	case SetModeResult::InvalidFullScreen:
		interface::Printf("...WARNING: fullscreen unavailable in this mode\n");
		return false;
	case SetModeResult::InvalidMode:
		interface::Printf("...WARNING: could not set the given mode (%d)\n", mode);
		return false;
	default:
		break;
	}

	return true;
}

float GetAspectRatio()
{
	return s_window.aspectRatio;
}

int GetRefreshRate()
{
	return s_window.refreshRate;
}

int GetWidth()
{
	return s_window.width;
}

int GetHeight()
{
	return s_window.height;
}

int IsFullscreen()
{
	return s_window.isFullscreen;
}

void Initialize()
{
	if (interface::Cvar_GetInteger("com_abnormalExit"))
	{
		interface::Cvar_Set("r_mode", util::VarArgs("%d", R_MODE_FALLBACK));
		interface::Cvar_Set("r_fullscreen", "0");
		interface::Cvar_Set("r_centerWindow", "0");
		interface::Cvar_Set("com_abnormalExit", "0");
	}

	// Create the window and set up the context
	if (StartDriverAndSetMode(g_cvars.mode.getInt(), g_cvars.fullscreen.getBool(), g_cvars.noborder.getBool()))
		goto success;

	if (g_cvars.noborder.getBool())
	{
		// Try again with a window border
		if (StartDriverAndSetMode(g_cvars.mode.getInt(), g_cvars.fullscreen.getBool(), false))
			goto success;
	}

	// Finally, try the default screen resolution
	if (g_cvars.mode.getInt() != R_MODE_FALLBACK)
	{
		interface::Printf("Setting r_mode %d failed, falling back on r_mode %d\n", g_cvars.mode.getInt(), R_MODE_FALLBACK);

		if (StartDriverAndSetMode(R_MODE_FALLBACK, false, false))
			goto success;
	}

	// Nothing worked, give up
	interface::FatalError("Could not load OpenGL subsystem");

success:
	// This depends on SDL_INIT_VIDEO, hence having it here
	interface::IN_Init(SDL_window);

	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	
	if (SDL_GetWindowWMInfo(SDL_window, &wmi) == SDL_FALSE)
	{
		interface::FatalError("SDL_GetWindowWMInfo: %s", SDL_GetError());
	}

	bgfx::PlatformData pd = {};
#ifdef WIN32
	pd.nwh = wmi.info.win.window;
#else
	pd.ndt = wmi.info.x11.display;
	pd.nwh = void*)(uintptr_t)wmi.info.x11.window;
#endif
	bgfx::setPlatformData(pd);
}

void SetGamma(const uint8_t *red, const uint8_t *green, const uint8_t *blue)
{
	uint16_t table[3][256];

	for (int i = 0; i < 256; i++)
	{
		table[0][i] = (((uint16_t)red[i]) << 8) | red[i];
		table[1][i] = (((uint16_t)green[i]) << 8) | green[i];
		table[2][i] = (((uint16_t)blue[i]) << 8) | blue[i];
	}

#ifdef _WIN32
	// Win2K and newer put this odd restriction on gamma ramps...
	for (int j = 0; j < 3; j++)
	{
		for (int i = 0; i < 128; i++)
		{
			if (table[j][i] >((128 + i) << 8))
				table[j][i] = (128 + i) << 8;
		}

		if (table[j][127] > 254 << 8)
			table[j][127] = 254 << 8;
	}
#endif

	// enforce constantly increasing
	for (int j = 0; j < 3; j++)
	{
		for (int i = 1; i < 256; i++)
		{
			if (table[j][i] < table[j][i - 1])
				table[j][i] = table[j][i - 1];
		}
	}

	if (SDL_SetWindowGammaRamp(SDL_window, table[0], table[1], table[2]) < 0)
	{
		interface::PrintWarningf("SDL_SetWindowGammaRamp() failed: %s\n", SDL_GetError());
	}
}


void Shutdown()
{
	interface::IN_Shutdown();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

} // namespace window
} // namespace renderer
