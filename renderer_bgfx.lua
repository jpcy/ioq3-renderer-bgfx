function createBgfxProject(bgfxPath, bxPath)
	project "bgfx"
	language "C++"
	kind "StaticLib"
	
	files
	{
		path.join(bgfxPath, "src/*.cpp"),
		path.join(bgfxPath, "src/*.h"),
		path.join(bgfxPath, "include/*.h")
	}
	
	excludes
	{
		path.join(bgfxPath, "src/amalgamated.cpp"),
		path.join(bgfxPath, "src/ovr.cpp"),
		path.join(bgfxPath, "src/glcontext_ppapi.cpp"),
		path.join(bgfxPath, "src/glcontext_glx.cpp"),
		path.join(bgfxPath, "src/glcontext_egl.cpp")
	}
	
	includedirs
	{
		path.join(bgfxPath, "include"),
		path.join(bgfxPath, "3rdparty"),
		path.join(bgfxPath, "3rdparty/dxsdk/include"),
		path.join(bgfxPath, "3rdparty/khronos"),
		bxPath,
		path.join(bxPath, "include"),
	}
	
	configuration "vs*"
		includedirs { path.join(bxPath, "include/compat/msvc") }
	configuration { "windows", "gmake" }
		buildoptions { "-std=c++0x" }
		includedirs { path.join(bxPath, "include/compat/mingw") }
	configuration {}
	
	defines { "__STDC_FORMAT_MACROS" }

	configuration "Debug"
		defines { "BGFX_CONFIG_DEBUG=1" }
end


function createRendererProject(bgfxPath, bxPath, ioq3Path, rendererPath, sdlLib32, sdlLib64)
	project "renderer_bgfx"
	language "C++"
	kind "SharedLib"
	
	configuration "x64"
		targetname "renderer_bgfx_x86_64"
	configuration "not x64"
		targetname "renderer_bgfx_x86"
	configuration {}
	
	configuration "vs*"
		pchheader "Precompiled.h"
		pchsource(path.join(rendererPath, "code/renderer_bgfx/Precompiled.cpp"))
	configuration {}
	
	defines
	{
		"USE_RENDERER_DLOPEN",
		"USE_LOCAL_HEADERS"
	}

	files
	{
		"%{prj.location}/dynamic/renderer_bgfx/bgfx_shader.c",
		"%{prj.location}/dynamic/renderer_bgfx/Common.c",
		"%{prj.location}/dynamic/renderer_bgfx/Fog_fragment.c",
		"%{prj.location}/dynamic/renderer_bgfx/Fog_vertex.c",
		"%{prj.location}/dynamic/renderer_bgfx/Generators.c",
		"%{prj.location}/dynamic/renderer_bgfx/Generic_fragment.c",
		"%{prj.location}/dynamic/renderer_bgfx/Generic_vertex.c",
		"%{prj.location}/dynamic/renderer_bgfx/Lighting.c",
		"%{prj.location}/dynamic/renderer_bgfx/TextureColor_fragment.c",
		"%{prj.location}/dynamic/renderer_bgfx/TextureColor_vertex.c",
		"%{prj.location}/dynamic/renderer_bgfx/varying_def.c",
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
		path.join(rendererPath, "shaders/*.sh")
	}
	
	filter("files:**.c or " .. path.getrelative(path.getabsolute("."), path.join(rendererPath, "code/math/*.cpp")))
		flags "NoPCH"
	filter {}
	
	vpaths
	{
		["shaders"] = path.join(rendererPath, "shaders/*.*"),
		["*"] = path.join(rendererPath, "code")
	}
	
	includedirs
	{
		path.join(ioq3Path, "code/SDL2/include"),
		path.join(bxPath, "include"),
		path.join(bgfxPath, "include"),
		path.join(bgfxPath, "src"), -- for bgfx_shader.sh
		path.join(rendererPath, "code/stb")
	}
	
	configuration "vs*"
		includedirs { path.join(bxPath, "include/compat/msvc") }
	configuration { "windows", "gmake" }
		includedirs { path.join(bxPath, "include/compat/mingw") }
	configuration {}
	
	links
	{
		"gdi32",
		"psapi",
		"OpenGL32",
		"d3dcompiler",
		
		-- Other projects
		"bgfx",
		"shaderc"
	}
	
	configuration(path.getrelative(path.getabsolute("."), path.join(rendererPath, "shaders/*.*")))
		buildmessage "Stringifying %{file.name}"
		buildcommands("cscript.exe \"" .. path.join(ioq3Path, "misc/msvc/glsl_stringify.vbs") .. "\" \"%{file.abspath}\" \"" .. path.join("%{prj.location}", "dynamic\\renderer_bgfx\\%{file.basename}.c\""))
		buildoutputs "%{prj.location}\\dynamic\\renderer_bgfx\\%{file.basename}.c"
	
	configuration "not x64"
		links(sdlLib32)
	configuration "x64"
		links(sdlLib64)
	configuration "vs*"
		buildoptions { "/wd\"4316\"", "/wd\"4351\"" } -- Silence some warnings
	configuration { "gmake" }
		buildoptions { "-std=c++14" }
		linkoptions
		{
			"-static-libgcc",
			"-static-libstdc++"
		}
end

--
-- Copyright 2010-2015 Branimir Karadzic. All rights reserved.
-- License: http://www.opensource.org/licenses/BSD-2-Clause
--
function createShadercProject(bgfxPath, bxPath, rendererPath)
	project "shaderc"
	language "C++"
	kind "StaticLib"
	
	local GLSL_OPTIMIZER = path.join(bgfxPath, "3rdparty/glsl-optimizer")
	local FCPP_DIR = path.join(rendererPath, "code/fcpp")

	removeflags
	{
		"OptimizeSpeed", -- GCC 4.9 -O2 + -fno-strict-aliasing don't work together...
	}
	
	configuration "vs*"
		includedirs
		{
			path.join(bxPath, "include/compat/msvc")
		}

		-- glsl-optimizer
		defines
		{
			"__STDC__",
			"__STDC_VERSION__=199901L",
			"strdup=_strdup",
			"alloca=_alloca",
			"isascii=__isascii"
		}

		buildoptions
		{
			"/wd4291", -- 'declaration' : no matching operator delete found; memory will not be freed if initialization throws an exception
			"/wd4996" -- warning C4996: 'strdup': The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name: _strdup.
		}
	configuration { "windows", "gmake" }
		buildoptions { "-std=c++0x" }
		includedirs { path.join(bxPath, "include/compat/mingw") }	
	configuration {}
	
	defines { "SHADERC_LIB" }

	-- fcpp
	defines
	{
		"NINCLUDE=64",
		"NWORK=65536",
		"NBUFF=65536",
		"OLD_PREPROCESSOR=0"
	}

	includedirs
	{
		path.join(bxPath, "include"),
		path.join(bgfxPath, "include"),
		path.join(bgfxPath, "tools/shaderc"),
		FCPP_DIR,
		path.join(GLSL_OPTIMIZER, "src"),
		path.join(GLSL_OPTIMIZER, "include"),
		path.join(GLSL_OPTIMIZER, "src/mesa"),
		path.join(GLSL_OPTIMIZER, "src/mapi"),
		path.join(GLSL_OPTIMIZER, "src/glsl"),
	}

	files
	{
		path.join(rendererPath, "code/shaderc/*.cpp"),
		path.join(rendererPath, "code/shaderc/*.h"),
		path.join(bgfxPath, "src/vertexdecl.**"),
		path.join(FCPP_DIR, "**.h"),
		path.join(FCPP_DIR, "cpp1.c"),
		path.join(FCPP_DIR, "cpp2.c"),
		path.join(FCPP_DIR, "cpp3.c"),
		path.join(FCPP_DIR, "cpp4.c"),
		path.join(FCPP_DIR, "cpp5.c"),
		path.join(FCPP_DIR, "cpp6.c"),
		path.join(FCPP_DIR, "cpp6.c"),
		path.join(GLSL_OPTIMIZER, "src/mesa/**.c"),
		path.join(GLSL_OPTIMIZER, "src/glsl/**.cpp"),
		path.join(GLSL_OPTIMIZER, "src/mesa/**.h"),
		path.join(GLSL_OPTIMIZER, "src/glsl/**.c"),
		path.join(GLSL_OPTIMIZER, "src/glsl/**.cpp"),
		path.join(GLSL_OPTIMIZER, "src/glsl/**.h"),
		path.join(GLSL_OPTIMIZER, "src/util/**.c"),
		path.join(GLSL_OPTIMIZER, "src/util/**.h"),
	}

	removefiles
	{
		path.join(GLSL_OPTIMIZER, "src/glsl/glcpp/glcpp.c"),
		path.join(GLSL_OPTIMIZER, "src/glsl/glcpp/tests/**"),
		path.join(GLSL_OPTIMIZER, "src/glsl/glcpp/**.l"),
		path.join(GLSL_OPTIMIZER, "src/glsl/glcpp/**.y"),
		path.join(GLSL_OPTIMIZER, "src/glsl/ir_set_program_inouts.cpp"),
		path.join(GLSL_OPTIMIZER, "src/glsl/main.cpp"),
		path.join(GLSL_OPTIMIZER, "src/glsl/builtin_stubs.cpp"),
	}
end
