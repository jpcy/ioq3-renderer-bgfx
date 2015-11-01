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

project "bgfx"
	language "C++"
	kind "StaticLib"
	
	files
	{
		path.join(BGFX_PATH, "src/*.cpp"),
		path.join(BGFX_PATH, "src/*.h"),
		path.join(BGFX_PATH, "include/*.h")
	}
	
	excludes
	{
		path.join(BGFX_PATH, "src/amalgamated.cpp"),
		path.join(BGFX_PATH, "src/ovr.cpp"),
		path.join(BGFX_PATH, "src/glcontext_ppapi.cpp"),
		path.join(BGFX_PATH, "src/glcontext_glx.cpp"),
		path.join(BGFX_PATH, "src/glcontext_egl.cpp")
	}
	
	includedirs
	{
		path.join(BGFX_PATH, "include"),
		path.join(BGFX_PATH, "3rdparty"),
		path.join(BGFX_PATH, "3rdparty/dxsdk/include"),
		path.join(BGFX_PATH, "3rdparty/khronos"),
		BX_PATH,
		path.join(BX_PATH, "include"),
		path.join(BX_PATH, "include/compat/msvc")
	}
	
	defines { "__STDC_FORMAT_MACROS" }

	configuration "Debug"
		defines { "BGFX_CONFIG_DEBUG=1" }
