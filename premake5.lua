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

if os.is("windows") then
	if not os.isdir(IOQ3_PATH) then
		print("ioquake3 not found at " .. IOQ3_PATH)
		os.exit()
	end
end

newaction
{
	trigger = "shaders",
	description = "Compile shaders",
	
	onStart = function()
		local renderers = nil
		
		if os.is("windows") then
			renderers = { "gl", "d3d11" }
		else
			renderers = { "gl" }
		end
		
		local tempOutputFilename = "build/tempoutput"
		local outputSourceFilename = "build/Shader.cpp"
		
		function compileShader(input, type, permutation, defines)
			io.write("Compiling " .. input .. "_" .. type)
			
			if permutation == nil then
				io.write("\n")
			else
				io.write(" " .. permutation .. "\n")
			end
			
			local inputFilename = "shaders/" .. input .. "_" .. type .. ".sc"
			
			-- Compile the shader for all renderers.
			for _,renderer in pairs(renderers) do
				local command = nil
				
				if os.is("windows") then
					command = "shaderc.exe"
				elseif os.is("linux") then
					command = "`./shaderc64"
				end
				
				local variableName = input .. "_"

				if permutation ~= nil then
					variableName = variableName .. permutation .. "_"
				end

				variableName = variableName .. type .. "_" .. renderer
				
				command = command .. " -i \"shaders;" .. path.join(BGFX_PATH, "src") .. "\" -f \"" .. inputFilename .. "\" -o \"" .. tempOutputFilename .. "\" --varyingdef shaders/varying.def.sc --bin2c \"" .. variableName .. "\" --type " .. type
			
				if defines ~= nil then
					command = command .. " --define " .. defines
				end
				
				if renderer == "gl" then
					command = command .. " --platform linux -p 140"
				elseif renderer == "d3d9" or renderer == "d3d11" then
					command = command .. " --platform windows"
				
					if type == "fragment" then
						command = command .. " -p ps_"
					else
						command = command .. " -p vs_"
					end
				
					if renderer == "d3d9" then
						command = command .. "3_0"
					elseif renderer == "d3d11" then
						command = command .. "4_0"
					end
					
					command = command .. " -O 3"
				end
				
				if os.is("linux") then
					command = command .. "`"
				end
				
				if os.execute(command) ~= 0 then
					local message = "\n" .. input .. " " .. type
					
					if permutation ~= nil then
						message = message .. " " .. permutation
					end
					
					message = message .. " " .. renderer .. "\n" .. command
					error(message)
				end
				
				-- Append the temp output file to the real output file.
				local tempFile = io.open(tempOutputFilename, "r")
				local tempContent = tempFile:read("*all")
				tempFile:close()
				local outputFile = io.open(outputSourceFilename, "a")
				outputFile:write("\n")
				outputFile:write(tempContent)
				outputFile:close()
			end
		end
		
		local fragmentShaders =
		{
			{ "AdaptedLuminance" },
			{ "Depth" },
			{ "Depth", "AlphaTest", "USE_ALPHA_TEST" },
			{ "Fog" },
			{ "FXAA" },
			{ "Generic" },
			{ "Generic", "AlphaTest", "USE_ALPHA_TEST" },
			{ "Generic", "AlphaTestSoftSprite", "USE_ALPHA_TEST;USE_SOFT_SPRITE" },
			{ "Generic", "SoftSprite", "USE_SOFT_SPRITE" },
			{ "LinearDepth" },
			{ "Luminance" },
			{ "LuminanceDownsample" },
			{ "Texture" },
			{ "TextureSingleChannel" },
			{ "TextureColor" },
			{ "ToneMap" }
		}
		
		local vertexShaders =
		{
			{ "Depth" },
			{ "Depth", "AlphaTest", "USE_ALPHA_TEST" },
			{ "Fog" },
			{ "Generic" },
			{ "Texture" }
		}
		
		function compileAllShaders()
			for _,v in pairs(fragmentShaders) do
				compileShader(v[1], "fragment", v[2], v[3])
			end
			
			for _,v in pairs(vertexShaders) do
				compileShader(v[1], "vertex", v[2], v[3])
			end
		end
		
		-- Make sure the build directory exists
		os.mkdir("build")
		
		-- Delete the output file so we can append to it.
		os.remove(outputSourceFilename)

		-- Compile the shaders.
		local ok, message = pcall(compileAllShaders)
		
		if not ok then
			print(message)
			return
		end
		
		-- Generate shader ID enums, writing them to the output header file.
		function writeEnum(of, data, name)
			of:write("struct " .. name .. "\n")
			of:write("{\n")
			of:write("\tenum Enum\n")
			of:write("\t{\n")
			
			for _,v in pairs(data) do
				of:write("\t\t" .. v[1])
				
				if v[2] ~= nil then
					of:write("_" .. v[2])
				end
				
				of:write(",\n")
			end
			
			of:write("\t\tNum\n")
			of:write("\t};\n")
			of:write("};\n\n")
		end
		
		local outputHeaderFilename = "build/Shader.h"
		local outputHeaderFile = io.open(outputHeaderFilename, "w")
		writeEnum(outputHeaderFile, fragmentShaders, "FragmentShaderId")
		writeEnum(outputHeaderFile, vertexShaders, "VertexShaderId")
		outputHeaderFile:close()

		-- Generate functions to map shader ID enums to source strings, appending them to the output source file.
		function writeSourceMap(of, data, renderer, name, nameLower)
			of:write(string.format("\nstatic std::array<const bgfx::Memory *, %sShaderId::Num> Get%sShaderSourceMap_%s()\n", name, name, renderer))
			of:write("{\n")
			of:write(string.format("\tstd::array<const bgfx::Memory *, %sShaderId::Num> mem;\n", name))
			
			for _,v in pairs(data) do
				local id = v[1]
				
				if v[2] ~= nil then
					id = id .. "_" .. v[2]
				end
			
				local source = string.format("%s_%s_%s", id, nameLower, renderer)
				of:write(string.format("\tmem[%sShaderId::%s] = bgfx::makeRef(%s, sizeof(%s));\n", name, id, source, source))
			end
			
			of:write("\treturn mem;\n")
			of:write("}\n")
		end
		
		local outputSourceFile = io.open(outputSourceFilename, "a")
		
		for _,renderer in pairs(renderers) do
			writeSourceMap(outputSourceFile, fragmentShaders, renderer, "Fragment", "fragment")
			writeSourceMap(outputSourceFile, vertexShaders, renderer, "Vertex", "vertex")
		end
		
		outputSourceFile:close()
		
		print("Done.")
    end,
}

solution "renderer_bgfx"
	configurations { "Release", "Debug" }
	location "build"
	
	if os.is64bit() and not os.is("windows") then
		platforms { "x86_64", "x86" }
	else
		platforms { "x86", "x86_64" }
	end
		
	startproject "renderer_bgfx"
	
	configuration "platforms:x86"
		architecture "x86"
		
	configuration "platforms:x86_64"
		architecture "x86_64"
	
	configuration "Debug"
		optimize "Debug"
		defines { "_DEBUG" }
		flags "Symbols"
		
	configuration { "Debug", "x86" }
		targetdir "build/bin_x86_debug"
		
	configuration { "Debug", "x86_64" }
		targetdir "build/bin_x64_debug"
		
	configuration "Release"
		optimize "Full"
		defines "NDEBUG"
		
	configuration { "Release", "x86" }
		targetdir "build/bin_x86"
		
	configuration { "Release", "x86_64" }
		targetdir "build/bin_x64"
		
	configuration "vs*"
		defines { "_CRT_SECURE_NO_DEPRECATE" }
		
	configuration { "vs*", "x86_64" }
		defines { "_WIN64", "__WIN64__" }
		
dofile("renderer_bgfx.lua")

if os.is("windows") then
	createRendererProject(BGFX_PATH, BX_PATH, RENDERER_PATH, path.join(IOQ3_PATH, "code/SDL2/include"), path.join(IOQ3_PATH, "code/libs/win32/libSDL2"), path.join(IOQ3_PATH, "code/libs/win64/libSDL264"))
else
	createRendererProject(BGFX_PATH, BX_PATH, RENDERER_PATH, nil, nil, nil)
end
