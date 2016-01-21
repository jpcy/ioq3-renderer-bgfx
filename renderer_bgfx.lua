function createRendererProject(bgfxPath, bxPath, rendererPath, sdlIncludeDir, sdlLib32, sdlLib64)
	project "renderer_bgfx"
	kind "SharedLib"
	language "C++"
	targetprefix ""
	
	defines
	{
		"__STDC_FORMAT_MACROS", -- bgfx
		"BGFX_CONFIG_RENDERER_OPENGL=31",
		"USE_RENDERER_DLOPEN"
	}

	files
	{
		path.join(bgfxPath, "src/amalgamated.cpp"),
		path.join(rendererPath, "code/qcommon/cm_public.h"),
		path.join(rendererPath, "code/qcommon/puff.c"),
		path.join(rendererPath, "code/qcommon/puff.h"),
		path.join(rendererPath, "code/qcommon/q_math.c"),
		path.join(rendererPath, "code/qcommon/q_platform.h"),
		path.join(rendererPath, "code/qcommon/q_shared.c"),
		path.join(rendererPath, "code/qcommon/q_shared.h"),
		path.join(rendererPath, "code/qcommon/qcommon.h"),
		path.join(rendererPath, "code/qcommon/qfiles.h"),
		path.join(rendererPath, "code/qcommon/surfaceflags.h"),
		path.join(rendererPath, "code/math/*.cpp"),
		path.join(rendererPath, "code/math/*.h"),
		path.join(rendererPath, "code/renderer_bgfx/*.cpp"),
		path.join(rendererPath, "code/renderer_bgfx/*.h"),
		path.join(rendererPath, "code/renderercommon/qgl.h"),
		path.join(rendererPath, "code/renderercommon/tr_common.h"),
		path.join(rendererPath, "code/renderercommon/tr_noise.c"),
		path.join(rendererPath, "code/renderercommon/tr_public.h"),
		path.join(rendererPath, "code/renderercommon/tr_types.h"),
		path.join(rendererPath, "shaders/*.sc"),
		path.join(rendererPath, "shaders/*.sh"),
		path.join(rendererPath, "code/qcommon/puff.c"),
		path.join(rendererPath, "code/qcommon/q_math.c"),
		path.join(rendererPath, "code/qcommon/q_shared.c"),
		path.join(rendererPath, "code/renderercommon/tr_noise.c")
	}
	
	includedirs
	{
		path.join(bxPath, "include"),
		path.join(bgfxPath, "include"),
		path.join(bgfxPath, "3rdparty"),
		path.join(bgfxPath, "3rdparty/dxsdk/include"),
		path.join(bgfxPath, "3rdparty/khronos"),
		path.join(rendererPath, "code/stb")
	}
	
	vpaths
	{
		["shaders"] = path.join(rendererPath, "shaders/*.*"),
		["*"] = path.join(rendererPath, "code")
	}
	
	-- Workaround os.outputof always being evaluated, even if in a configuration block that doesn't apply to the current environment.
	local linuxSdlCflags = nil
	local linuxArchDefine = nil
	
	if os.is("linux") then
		linuxSdlCflags = os.outputof("pkg-config --silence-errors --cflags sdl2")
		linuxArchDefine = "ARCH_STRING=" .. os.outputof("uname -m")
	end
	
	configuration "Debug"
		defines "BGFX_CONFIG_DEBUG=1"
	
	configuration "gmake"
		buildoptions "-std=c++14"
		
	configuration "linux"
		buildoptions
		{
			linuxSdlCflags,
			"-std=c++14"
		}

		defines(linuxArchDefine)

		links
		{
			"GL",
			"pthread",
			"SDL2",
			"X11"
		}
		
		linkoptions "-Wl,--no-undefined"
		
	configuration "vs*"
		buildoptions { "/wd\"4316\"", "/wd\"4351\"" } -- Silence some warnings
		includedirs(path.join(bxPath, "include/compat/msvc"))
		pchheader "Precompiled.h"
		pchsource(path.join(rendererPath, "code/renderer_bgfx/Precompiled.cpp"))
		
	configuration "windows"
		defines { "BGFX_CONFIG_RENDERER_DIRECT3D11=1", "BGFX_CONFIG_RENDERER_DIRECT3D12=1" }
		includedirs(sdlIncludeDir)
		links { "d3dcompiler", "gdi32", "OpenGL32", "psapi" }
		
	configuration { "windows", "gmake" }
		includedirs(path.join(bxPath, "include/compat/mingw"))
		linkoptions { "-static-libgcc", "-static-libstdc++" }
	
	configuration { "windows", "gmake", "x86" }
		gccprefix "i686-w64-mingw32-"
	
	configuration { "windows", "gmake", "x86_64" }
		gccprefix "x86_64-w64-mingw32-"

	configuration { "windows", "x86" }
		links(sdlLib32)
		
	configuration { "windows", "x86_64" }
		links(sdlLib64)

	configuration "x86"
		targetname "renderer_bgfx_x86"
		
	configuration "x86_64"
		targetname "renderer_bgfx_x86_64"
		
	configuration {}
	
	filter("files:not " .. path.getrelative(path.getabsolute("."), path.join(rendererPath, "code/renderer_bgfx/*.cpp")))
		flags "NoPCH"
	filter {}
end
