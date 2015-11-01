local BX_PATH = path.join(path.getabsolute(".."), "bx")

if not os.isdir(BX_PATH) then
	print("bx not found at " .. BX_PATH)
	os.exit()
end

local BGFX_PATH = path.join(path.getabsolute(".."), "bgfx")

if not os.isdir(BGFX_PATH) then
	print("bgfx not found at " .. BGFX_PATH)
	os.exit()
end


--
-- Copyright 2010-2015 Branimir Karadzic. All rights reserved.
-- License: http://www.opensource.org/licenses/BSD-2-Clause
--
project "shaderc"
	language "C++"
	kind "StaticLib"
	
	local GLSL_OPTIMIZER = path.join(BGFX_PATH, "3rdparty/glsl-optimizer")
	local FCPP_DIR = "code/fcpp"

	removeflags
	{
		"OptimizeSpeed", -- GCC 4.9 -O2 + -fno-strict-aliasing don't work together...
	}
	
	configuration "vs*"
		includedirs
		{
			path.join(BX_PATH, "include/compat/msvc"),
			path.join(GLSL_OPTIMIZER, "src/glsl/msvc")
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
		path.join(BX_PATH, "include"),
		path.join(BGFX_PATH, "include"),
		path.join(BGFX_PATH, "tools/shaderc"),
		FCPP_DIR,
		path.join(GLSL_OPTIMIZER, "src"),
		path.join(GLSL_OPTIMIZER, "include"),
		path.join(GLSL_OPTIMIZER, "src/mesa"),
		path.join(GLSL_OPTIMIZER, "src/mapi"),
		path.join(GLSL_OPTIMIZER, "src/glsl"),
	}

	files
	{
		"code/shaderc/*.cpp",
		"code/shaderc/*.h",
		path.join(BGFX_PATH, "src/vertexdecl.**"),
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
