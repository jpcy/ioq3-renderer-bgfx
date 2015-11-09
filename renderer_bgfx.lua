local IOQ3_PATH = path.join(path.getabsolute(".."), "ioq3")

if not os.isdir(IOQ3_PATH) then
	print("ioquake3 not found at " .. IOQ3_PATH)
	os.exit()
end

local IOQ3_PREMAKE_MSVC_PATH = path.join(path.getabsolute(".."), "ioq3-premake-msvc")

if not os.isdir(IOQ3_PREMAKE_MSVC_PATH) then
	print("ioq3-premake-msvc not found at " .. IOQ3_PREMAKE_MSVC_PATH)
	os.exit()
end

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

project "renderer_bgfx"
	language "C++"
	kind "SharedLib"
	pchheader "Precompiled.h"
	pchsource "code/renderer_bgfx/Precompiled.cpp"
	
	configuration "x64"
		targetname "renderer_bgfx_x86_64"
	configuration "not x64"
		targetname "renderer_bgfx_x86"
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
		"code/qcommon/cm_public.h",
		"code/qcommon/puff.c",
		"code/qcommon/puff.h",
		"code/qcommon/q_math.c",
		"code/qcommon/q_platform.h",
		"code/qcommon/q_shared.c",
		"code/qcommon/q_shared.h",
		"code/qcommon/qcommon.h",
		"code/qcommon/qfiles.h",
		"code/qcommon/surfaceflags.h",
		"code/math/*.cpp",
		"code/math/*.h",
		"code/renderer_bgfx/*.cpp",
		"code/renderer_bgfx/*.h",
		"code/renderercommon/qgl.h",
		"code/renderercommon/tr_common.h",
		"code/renderercommon/tr_noise.c",
		"code/renderercommon/tr_public.h",
		"code/renderercommon/tr_types.h",
		"shaders/*.sc",
		"shaders/*.sh"		
	}
	
	filter("files:**.c or code/math/*.cpp")
		flags "NoPCH"
	filter {}
	
	vpaths
	{
		["shaders"] = { "shaders/*.sc", "shaders/*.sh" },
		["*"] = "code"
	}
	
	includedirs
	{
		path.join(IOQ3_PATH, "code/SDL2/include"),
		path.join(BX_PATH, "include"),
		path.join(BGFX_PATH, "include"),
		path.join(BGFX_PATH, "src"), -- for bgfx_shader.sh
		"code/stb"
	}
	
	links
	{
		"psapi",
		"OpenGL32",
		"d3dcompiler",
		
		-- Other projects
		"bgfx",
		"shaderc"
	}
	
	configuration "shaders/*.*"
		buildmessage "Stringifying %{file.name}"
		buildcommands("cscript.exe \"" .. path.join(IOQ3_PATH, "misc/msvc/glsl_stringify.vbs") .. "\" //Nologo \"%{file.relpath}\" \"%{prj.location}\\dynamic\\renderer_bgfx\\%{file.basename}.c\"")
		buildoutputs "%{prj.location}\\dynamic\\renderer_bgfx\\%{file.basename}.c"
	
	configuration "not x64"
		links(path.join(IOQ3_PREMAKE_MSVC_PATH, "SDL2/x86/SDL2.lib"))
	configuration "x64"
		links(path.join(IOQ3_PREMAKE_MSVC_PATH, "SDL2/x64/SDL2.lib"))
	configuration {}
	
	buildoptions { "/wd\"4316\"", "/wd\"4351\"" } -- Silence some warnings
