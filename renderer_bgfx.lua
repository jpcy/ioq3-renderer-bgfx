function rendererProject(engine, rendererPath)
	project "renderer_bgfx"
	kind "SharedLib"
	language "C++"
	rtti "Off"
	targetprefix ""
	
	defines
	{
		"__STDC_CONSTANT_MACROS",
		"__STDC_FORMAT_MACROS",
		"__STDC_LIMIT_MACROS",
		"BGFX_CONFIG_RENDERER_OPENGL=31",
		"USE_RENDERER_DLOPEN"
	}
	
	local bxPath = path.join(rendererPath, "code/bx")
	local bimgPath = path.join(rendererPath, "code/bimg")
	local bgfxPath = path.join(rendererPath, "code/bgfx")
	
	files
	{
		path.join(bxPath, "include/bx/*.h"),
		path.join(bxPath, "include/bx/inline/*.inl"),
		path.join(bxPath, "src/allocator.cpp"),
		path.join(bxPath, "src/bx.cpp"),
		path.join(bxPath, "src/commandline.cpp"),
		path.join(bxPath, "src/crtnone.cpp"),
		path.join(bxPath, "src/debug.cpp"),
		path.join(bxPath, "src/dtoa.cpp"),
		path.join(bxPath, "src/file.cpp"),
		path.join(bxPath, "src/filepath.cpp"),
		path.join(bxPath, "src/math.cpp"),
		path.join(bxPath, "src/mutex.cpp"),
		path.join(bxPath, "src/os.cpp"),
		path.join(bxPath, "src/process.cpp"),
		path.join(bxPath, "src/semaphore.cpp"),
		path.join(bxPath, "src/sort.cpp"),
		path.join(bxPath, "src/string.cpp"),
		path.join(bxPath, "src/thread.cpp"),
		path.join(bxPath, "src/timer.cpp"),
		path.join(bimgPath, "include/bimg/*.h"),
		path.join(bimgPath, "src/image.cpp"),
		path.join(bimgPath, "src/image_decode.cpp"),
		path.join(bgfxPath, "include/bgfx/*.h"),
		path.join(bgfxPath, "src/bgfx.cpp"),
		--path.join(bgfxPath, "src/glcontext_egl.cpp"),
		path.join(bgfxPath, "src/glcontext_glx.cpp"),
		path.join(bgfxPath, "src/glcontext_wgl.cpp"),
		path.join(bgfxPath, "src/hmd.cpp"),
		--path.join(bgfxPath, "src/hmd_ovr.cpp"),
		--path.join(bgfxPath, "src/hmd_openvr.cpp"),
		path.join(bgfxPath, "src/debug_renderdoc.cpp"),
		path.join(bgfxPath, "src/renderer_d3d9.cpp"),
		path.join(bgfxPath, "src/renderer_d3d11.cpp"),
		path.join(bgfxPath, "src/renderer_d3d12.cpp"),
		path.join(bgfxPath, "src/renderer_noop.cpp"),
		path.join(bgfxPath, "src/renderer_gl.cpp"),
		path.join(bgfxPath, "src/renderer_vk.cpp"),
		path.join(bgfxPath, "src/renderer_gnm.cpp"),
		path.join(bgfxPath, "src/shader_dxbc.cpp"),
		path.join(bgfxPath, "src/shader_dx9bc.cpp"),
		path.join(bgfxPath, "src/shader_spirv.cpp"),
		path.join(bgfxPath, "src/topology.cpp"),
		path.join(bgfxPath, "src/vertexdecl.cpp"),
		path.join(bgfxPath, "src/*.h"),
		path.join(rendererPath, "code/math/*.cpp"),
		path.join(rendererPath, "code/math/*.h"),
		path.join(rendererPath, "code/renderer_bgfx/*.cpp"),
		path.join(rendererPath, "code/renderer_bgfx/*.h"),
		path.join(rendererPath, "shaders/*.hlsl"),
		path.join(rendererPath, "shaders/*.sc"),
		path.join(rendererPath, "shaders/*.sh"),
	}
	
	includedirs
	{
		path.join(bxPath, "include"),
		path.join(bimgPath, "include"),
		path.join(bimgPath, "3rdparty"),
		path.join(bimgPath, "3rdparty/nvtt"),
		path.join(bimgPath, "3rdparty/iqa/include"),
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
	
	if engine == "ioq3" then
		defines "ENGINE_IOQ3"
		
		configuration "x86"
			targetname "renderer_bgfx_x86"
		configuration "x86_64"
			targetname "renderer_bgfx_x86_64"
	elseif engine == "iortcw" then
		defines "ENGINE_IORTCW"
		
		configuration "x86"
			targetname "renderer_sp_bgfx_x86"
		configuration "x86_64"
			targetname "renderer_sp_bgfx_x86_64"
	end
	
	configuration "Debug"
		defines "BGFX_CONFIG_DEBUG=1"
	
	configuration "gmake"
		buildoptions "-std=c++11"
		
	configuration "linux"
		buildoptions
		{
			linuxSdlCflags,
			"-std=c++11"
		}

		defines(linuxArchDefine)

		links
		{
			"dl",
			"GL",
			"pthread",
			"rt",
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
		links { "d3dcompiler", "gdi32", "OpenGL32", "psapi" }
		
	configuration { "windows", "gmake" }
		includedirs(path.join(bxPath, "include/compat/mingw"))
		linkoptions { "-static-libgcc", "-static-libstdc++", "-Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic" }
		
	configuration { "windows", "gmake", "mingw=mingw" }
		gccprefix "mingw32-"
		
	configuration { "windows", "gmake", "mingw=mingw-pc", "x86" }
		gccprefix "i686-pc-mingw32-"
	
	configuration { "windows", "gmake", "mingw=mingw-w64", "x86" }
		gccprefix "i686-w64-mingw32-"
		
	configuration { "windows", "gmake", "mingw=mingw-pc", "x86_64" }
		gccprefix "x86_64-pc-mingw32-"
	
	configuration { "windows", "gmake", "mingw=mingw-w64", "x86_64" }
		gccprefix "x86_64-w64-mingw32-"

	configuration {}
	
	filter("files:not " .. path.getrelative(path.getabsolute("."), path.join(rendererPath, "code/renderer_bgfx/*.cpp")))
		flags "NoPCH"
	filter {}
end
