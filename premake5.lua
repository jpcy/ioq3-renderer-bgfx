newaction
{
	trigger = "shaders",
	description = "Compile shaders",
	
	onStart = function()
		os.mkdir("build/shaders")
		dofile("shader.lua")
		compileShader("Fog", "fragment")
		compileShader("Fog", "vertex")
		compileShader("Generic", "fragment")
		compileShader("Generic", "fragment", "AlphaTest", "#define USE_ALPHA_TEST")
		compileShader("Generic", "vertex")
		compileShader("TextureColor", "fragment")
		compileShader("TextureColor", "vertex")
    end,
	
	onEnd = function()
        print("Done.")
    end
}

if _ACTION == nil or _ACTION == "shaders" then
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
local RENDERER_PATH = path.getabsolute(".")

os.mkdir("build/bin_x86")
os.mkdir("build/bin_x64")
os.mkdir("build/bin_debug_x86")
os.mkdir("build/bin_debug_x64")
os.mkdir("build/shaders")
	
if os.get() == "windows" then
	if not os.isdir(IOQ3_PATH) then
		print("ioquake3 not found at " .. IOQ3_PATH)
		os.exit()
	end
end

solution "renderer_bgfx"
	location "build"
	startproject "renderer_bgfx"
	platforms { "native", "x32", "x64" }
	configurations { "Debug", "Release" }
	
	configuration "Debug"
		optimize "Debug"
		defines { "_DEBUG" }
		flags "Symbols"
		
	configuration { "Debug", "not x64" }
		targetdir "build/bin_debug_x86"
		
	configuration { "Debug", "x64" }
		targetdir "build/bin_debug_x64"
		
	configuration "Release"
		optimize "Full"
		defines "NDEBUG"
		
	configuration { "Release", "not x64" }
		targetdir "build/bin_x86"
		
	configuration { "Release", "x64" }
		targetdir "build/bin_x64"
		
	configuration "vs*"
		defines { "_CRT_SECURE_NO_DEPRECATE" }
		
	configuration { "vs*", "x64" }
		defines { "_WIN64", "__WIN64__" }

dofile("renderer_bgfx.lua")

if os.get() == "windows" then
	createRendererProject(BGFX_PATH, BX_PATH, RENDERER_PATH, path.join(IOQ3_PATH, "code/SDL2/include"), path.join(IOQ3_PATH, "code/libs/win32/libSDL2"), path.join(IOQ3_PATH, "code/libs/win64/libSDL264"))
else
	createRendererProject(BGFX_PATH, BX_PATH, RENDERER_PATH, nil, nil, nil)
end
