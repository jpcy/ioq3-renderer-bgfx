function createBgfxProject(bgfxPath, bxPath)
	project "bgfx"
	kind "StaticLib"
	language "C++"
	defines "__STDC_FORMAT_MACROS"
	
	files
	{
		path.join(bgfxPath, "src/*.cpp"),
		path.join(bgfxPath, "src/*.h"),
		path.join(bgfxPath, "include/*.h")
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
	
	removefiles
	{
		path.join(bgfxPath, "src/amalgamated.cpp"),
		path.join(bgfxPath, "src/ovr.cpp"),
		path.join(bgfxPath, "src/glcontext_ppapi.cpp"),
		path.join(bgfxPath, "src/glcontext_glx.cpp"),
		path.join(bgfxPath, "src/glcontext_egl.cpp")
	}
	
	configuration "Debug"
		defines "BGFX_CONFIG_DEBUG=1"
		
	configuration "gmake"
		buildoptions "-std=c++0x"
		
	configuration "linux"
		buildoptions "-fPIC"
		
	configuration "vs*"
		includedirs(path.join(bxPath, "include/compat/msvc"))
		
	configuration { "windows", "gmake" }
		includedirs(path.join(bxPath, "include/compat/mingw"))
end


function createRendererProject(bgfxPath, bxPath, rendererPath, sdlIncludeDir, sdlLib32, sdlLib64)
	project "renderer_bgfx"
	kind "SharedLib"
	language "C++"
	targetprefix ""
	
	defines "USE_RENDERER_DLOPEN"

	files
	{
		"%{prj.location}/dynamic/renderer_bgfx/bgfx_shader.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/Common.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/Fog_fragment.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/Fog_vertex.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/Generators.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/Generic_fragment.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/Generic_vertex.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/TextureColor_fragment.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/TextureColor_vertex.cpp",
		"%{prj.location}/dynamic/renderer_bgfx/varying_def.cpp",
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
		path.join(rendererPath, "code/stb")
	}
	
	links { "bgfx", "shaderc" }
	
	vpaths
	{
		["shaders"] = path.join(rendererPath, "shaders/*.*"),
		["*"] = path.join(rendererPath, "code")
	}
	
	-- Workaround os.outputof always being evaluated, even if in a configuration block that doesn't apply to the current environment.
	local linuxSdlCflags = nil
	local linuxArchDefine = nil
	
	if os.get() == "linux" then
		linuxSdlCflags = os.outputof("pkg-config --silence-errors --cflags sdl2")
		linuxArchDefine = "ARCH_STRING=" .. os.outputof("uname -m")
	end
	
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
			"SDL2"
		}
		
		removefiles
		{
			path.join(rendererPath, "code/qcommon/puff.c"),
			path.join(rendererPath, "code/qcommon/q_math.c"),
			path.join(rendererPath, "code/qcommon/q_shared.c"),
			path.join(rendererPath, "code/renderercommon/tr_noise.c")
		}
		
	configuration { "linux", path.getrelative(path.getabsolute("."), path.join(rendererPath, "shaders/*.*")) }
		local outputFile = path.join("%{prj.location}", "dynamic/renderer_bgfx/%{file.basename}.cpp")
		buildmessage "Stringifying %{file.name}"
		buildcommands
		{
			'rm -f ' .. outputFile,
			'echo \'const char *fallbackShader_%{file.basename} = R"END(\' >> ' .. outputFile,
			'cat %{file.abspath} >> ' .. outputFile,
			'echo \')END";\' >> ' .. outputFile
		}
		buildoutputs(outputFile)
		
	configuration "vs*"
		buildoptions { "/wd\"4316\"", "/wd\"4351\"" } -- Silence some warnings
		includedirs(path.join(bxPath, "include/compat/msvc"))
		pchheader "Precompiled.h"
		pchsource(path.join(rendererPath, "code/renderer_bgfx/Precompiled.cpp"))
		
	configuration "windows"
		includedirs(sdlIncludeDir)
		links { "d3dcompiler", "gdi32", "OpenGL32", "psapi" }
		
	configuration { "windows", "gmake" }
		includedirs(path.join(bxPath, "include/compat/mingw"))
		linkoptions { "-static-libgcc", "-static-libstdc++" }
		
	configuration { "windows", "x64" }
		links(sdlLib64)
		
	configuration { "windows", "not x64" }
		links(sdlLib32)
		
	configuration { "windows", path.getrelative(path.getabsolute("."), path.join(rendererPath, "shaders/*.*")) }
		local outputFile = path.join("%{prj.location}", "dynamic/renderer_bgfx/%{file.basename}.cpp")
		buildmessage "Stringifying %{file.name}"
		buildcommands('cscript.exe "' .. path.join(rendererPath, "glsl_stringify.vbs") .. '" "%{file.abspath}" "' .. outputFile .. '"')
		buildoutputs(outputFile)
		
	configuration "x64"
		targetname "renderer_bgfx_x86_64"
		
	configuration "not x64"
		targetname "renderer_bgfx_x86"
		
	configuration {}
	
	filter("files:not " .. path.getrelative(path.getabsolute("."), path.join(rendererPath, "code/renderer_bgfx/*.cpp")))
		flags "NoPCH"
	filter {}
end

--
-- Copyright 2010-2015 Branimir Karadzic. All rights reserved.
-- License: http://www.opensource.org/licenses/BSD-2-Clause
--
function createShadercProject(bgfxPath, bxPath, rendererPath)
	project "shaderc"
	kind "StaticLib"
	language "C++"
	
	local GLSL_OPTIMIZER = path.join(bgfxPath, "3rdparty/glsl-optimizer")
	local FCPP_DIR = path.join(rendererPath, "code/fcpp")

	defines "SHADERC_LIB"
	defines { "NINCLUDE=64", "NWORK=65536", "NBUFF=65536", "OLD_PREPROCESSOR=0" } -- fcpp
	
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
	
	-- GCC 4.9 -O2 + -fno-strict-aliasing don't work together...
	removeflags "OptimizeSpeed"
	
	configuration "gmake"
		buildoptions "-std=c++0x"
		
	configuration "linux"
		buildoptions "-fPIC"
		removefiles(path.join(rendererPath, "code/shaderc/shaderc_hlsl.cpp"))
		
	configuration "vs*"
		includedirs(path.join(bxPath, "include/compat/msvc"))
		
		-- glsl-optimizer
		defines { "__STDC__", "__STDC_VERSION__=199901L", "strdup=_strdup", "alloca=_alloca", "isascii=__isascii" }

		buildoptions
		{
			"/wd4291", -- 'declaration' : no matching operator delete found; memory will not be freed if initialization throws an exception
			"/wd4996" -- warning C4996: 'strdup': The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name: _strdup.
		}
		
	configuration { "windows", "gmake" }
		includedirs(path.join(bxPath, "include/compat/mingw"))
end
