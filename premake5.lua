if _ACTION == nil then
	return
end

local BGFX_PATH = path.join(path.getabsolute(".."), "bgfx")

if not os.isdir(BGFX_PATH) then
	print("bgfx not found at " .. BGFX_PATH)
	os.exit()
end

local BX_PATH = path.join(path.getabsolute(".."), "bx")

if not os.isdir(BX_PATH) then
	print("bx not found at " .. BX_PATH)
	os.exit()
end

local IOQ3_PATH = path.join(path.getabsolute(".."), "ioq3")

if not os.isdir(IOQ3_PATH) then
	print("ioquake3 not found at " .. IOQ3_PATH)
	os.exit()
end

local RENDERER_PATH = path.getabsolute(".")
	
if os.get() == "windows" then
	os.mkdir("build/bin_x86")
	os.mkdir("build/bin_x64")
	os.mkdir("build/bin_debug_x86")
	os.mkdir("build/bin_debug_x64")
	os.mkdir("build/dynamic/renderer_bgfx")
	os.copyfile("D3DCompiler_47.dll", "build/bin_x86/D3DCompiler_47.dll")
	os.copyfile("D3DCompiler_47.dll", "build/bin_x64/D3DCompiler_47.dll")
	os.copyfile("D3DCompiler_47.dll", "build/bin_debug_x86/D3DCompiler_47.dll")
	os.copyfile("D3DCompiler_47.dll", "build/bin_debug_x64/D3DCompiler_47.dll")
end

solution "renderer_bgfx"
	language "C"
	location "build"
	startproject "renderer_bgfx"
	platforms { "native", "x32", "x64" }
	configurations { "Debug", "Release" }
	
	configuration "vs*"
		defines { "_CRT_SECURE_NO_DEPRECATE" }
	
	configuration "x64"
		defines { "_WIN64", "__WIN64__" }
			
	configuration "Debug"
		optimize "Debug"
		defines { "_DEBUG" }
		flags "Symbols"
				
	configuration "Release"
		optimize "Full"
		defines "NDEBUG"
		
	configuration { "Debug", "not x64" }
		targetdir "build/bin_debug_x86"
		
	configuration { "Release", "not x64" }
		targetdir "build/bin_x86"
		
	configuration { "Debug", "x64" }
		targetdir "build/bin_debug_x64"
		
	configuration { "Release", "x64" }
		targetdir "build/bin_x64"

dofile("renderer_bgfx.lua")
createBgfxProject(BGFX_PATH, BX_PATH)
createRendererProject(BGFX_PATH, BX_PATH, IOQ3_PATH, RENDERER_PATH, path.join(IOQ3_PATH, "code/libs/win32/libSDL2"), path.join(IOQ3_PATH, "code/libs/win64/libSDL264"))
createShadercProject(BGFX_PATH, BX_PATH, RENDERER_PATH)
