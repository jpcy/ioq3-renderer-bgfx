local BGFX_PATH = path.join(path.getabsolute(".."), "bgfx")

if _ACTION ~= nil and not os.isdir(BGFX_PATH) then
	print("bgfx not found at " .. BGFX_PATH)
	os.exit()
end

newaction
{
	trigger = "shaders",
	description = "Compile shaders",
	
	onStart = function()
		-- No bitwise and in Lua 5.1
		function isBitSet(a, b)
			if a == 1 then
				if b == 1 then return true end
			elseif a == 2 then
				if b == 2 then return true end
			elseif a == 3 then
				if b == 1 or b == 2 then return true end
			elseif a == 4 then
				if b == 4 then return true end
			elseif a == 5 then
				if b == 1 or b == 4 then return true end
			elseif a == 6 then
				if b == 2 or b == 4 then return true end
			elseif a == 7 then
				if b == 1 or b == 2 or b == 4 then return true end
			elseif a == 8 then
				if b == 8 then return true end
			end
			
			return false
		end
		
		-- { name, { variantName, variantDefines } }
		-- becomes
		-- { name, variant1Name, variant1Defines }
		-- { name, variant2Name, variant2Defines }
		-- { name, variant1Name .. variant2Name, variant1Defines .. ";" .. variant2Defines }
		-- etc. for all variant combinations/permutations.
		function expandShaderVariants(shaders)
			local expandedShaders = {}
			local index = 1
		
			for _,shader in pairs(shaders) do
				expandedShaders[index] = { shader[1] }
				index = index + 1
				local variants = shader[2]
			
				if variants ~= nil then
					local n = #variants
				
					for i=0,2^n-1 do
						local concatVariant = ""
						local concatDefines = ""
					
						for vi,variant in ipairs(variants) do
							if isBitSet(i, 2^(vi-1)) then
								concatVariant = concatVariant .. variant[1]
								
								if concatDefines ~= "" then
									concatDefines = concatDefines .. ";"
								end
								
								concatDefines = concatDefines .. variant[2]
							end
						end
						
						if concatVariant ~= "" then
							expandedShaders[index] = { shader[1], concatVariant, concatDefines }
							index = index + 1
						end
					end
				end
			end
			
			return expandedShaders
		end
	
		-- Compile an individual shader for each renderer, appending the output to a file.
		function compileShader(input, type, variant, defines, outputFilename, renderers)
			local tempOutputFilename = "build/tempoutput"
			io.write("Compiling " .. input .. "_" .. type)
			
			if variant == nil then
				io.write("\n")
			else
				io.write(" " .. variant .. "\n")
			end
			
			local inputFilename = string.format("shaders/%s_%s.sc", input, type)
			
			-- Compile the shader for all renderers.
			for _,renderer in pairs(renderers) do
				local command = nil
				
				if os.is("windows") then
					command = "shaderc.exe"
				elseif os.is("linux") then
					command = "`./shaderc64"
				end
				
				local variableName = input .. "_"

				if variant ~= nil then
					variableName = variableName .. variant .. "_"
				end

				variableName = variableName .. type .. "_" .. renderer
				
				command = command .. string.format(" -i \"shaders;%s\" -f \"%s\" -o \"%s\" --varyingdef shaders/varying.def.sc --bin2c \"%s\" --type %s", path.join(BGFX_PATH, "src"), inputFilename, tempOutputFilename, variableName, type)
			
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
					
					if variant ~= nil then
						message = message .. " " .. variant
					end
					
					message = message .. " " .. renderer .. "\n" .. command
					error(message)
				end
				
				-- Append the temp output file to the real output file.
				local tempFile = io.open(tempOutputFilename, "r")
				local tempContent = tempFile:read("*all")
				tempFile:close()
				local outputFile = io.open(outputFilename, "a")
				outputFile:write("\n")
				outputFile:write(tempContent)
				outputFile:close()
			end
		end
		
		local renderers = nil
		
		if os.is("windows") then
			renderers = { "gl", "d3d11" }
		else
			renderers = { "gl" }
		end
		
		local depthFragmentVariants =
		{
			{ "AlphaTest", "USE_ALPHA_TEST" }
		}
		
		local depthVertexVariants =
		{
			{ "AlphaTest", "USE_ALPHA_TEST" },
			{ "DepthRange", "USE_DEPTH_RANGE" }
		}
		
		local fogVertexVariants =
		{
			{ "DepthRange", "USE_DEPTH_RANGE" }
		}
		
		local genericFragmentVariants =
		{
			{ "AlphaTest", "USE_ALPHA_TEST" },
			{ "DynamicLights", "USE_DYNAMIC_LIGHTS" },
			{ "SoftSprite", "USE_SOFT_SPRITE" }
		}
		
		local genericVertexVariants =
		{
			{ "DepthRange", "USE_DEPTH_RANGE" }
		}
		
		local fragmentShaders =
		{
			{ "AdaptedLuminance" },
			{ "Color" },
			{ "Depth", depthFragmentVariants },
			{ "Fog" },
			{ "FXAA" },
			{ "Generic", genericFragmentVariants },
			{ "LinearDepth" },
			{ "Luminance" },
			{ "LuminanceDownsample" },
			{ "SMAABlendingWeightCalculation" },
			{ "SMAAEdgeDetection" },
			{ "SMAANeighborhoodBlending" },
			{ "Texture" },
			{ "TextureSingleChannel" },
			{ "TextureColor" },
			{ "ToneMap" }
		}
		
		local vertexShaders =
		{
			{ "Color" },
			{ "Depth", depthVertexVariants },
			{ "Fog", fogVertexVariants },
			{ "Generic", genericVertexVariants },
			{ "SMAABlendingWeightCalculation" },
			{ "SMAAEdgeDetection" },
			{ "SMAANeighborhoodBlending" },
			{ "Texture" }
		}
		
		local outputSourceFilename = "build/Shader.cpp"
		
		-- Make sure the build directory exists
		os.mkdir("build")
		
		-- Delete the output file so we can append to it.
		os.remove(outputSourceFilename)
		
		-- Expand shader lists so each variant has a single entry.
		local expandedFragmentShaders = expandShaderVariants(fragmentShaders)
		local expandedVertexShaders = expandShaderVariants(vertexShaders)

		-- Compile the shaders.
		local ok, message = pcall(function()
			for _,v in pairs(expandedFragmentShaders) do
				compileShader(v[1], "fragment", v[2], v[3], outputSourceFilename, renderers)
			end
			
			for _,v in pairs(expandedVertexShaders) do
				compileShader(v[1], "vertex", v[2], v[3], outputSourceFilename, renderers)
			end
		end)
		
		if not ok then
			print(message)
			return
		end
		
		-- Generate shader ID and variant enums, writing them to the output header file.
		function writeShaderIdEnum(of, data, name)
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
		
		function writeShaderVariantEnum(of, data, name)
			of:write("struct " .. name .. "ShaderVariant\n")
			of:write("{\n")
			of:write("\tenum\n")
			of:write("\t{\n")
			local i = 0
			
			for _,v in pairs(data) do
				of:write(string.format("\t\t%s = 1 << %d,\n", v[1], i))
				i = i + 1
			end
			
			of:write(string.format("\t\tNum = 1 << %d\n", i))
			of:write("\t};\n")
			of:write("};\n\n")
		end
		
		local outputHeaderFilename = "build/Shader.h"
		local outputHeaderFile = io.open(outputHeaderFilename, "w")
		writeShaderIdEnum(outputHeaderFile, expandedFragmentShaders, "FragmentShaderId")
		writeShaderIdEnum(outputHeaderFile, expandedVertexShaders, "VertexShaderId")
		writeShaderVariantEnum(outputHeaderFile, genericFragmentVariants, "GenericFragment")
		writeShaderVariantEnum(outputHeaderFile, genericVertexVariants, "GenericVertex")
		writeShaderVariantEnum(outputHeaderFile, fogVertexVariants, "FogVertex")
		writeShaderVariantEnum(outputHeaderFile, depthFragmentVariants, "DepthFragment")
		writeShaderVariantEnum(outputHeaderFile, depthVertexVariants, "DepthVertex")
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
			writeSourceMap(outputSourceFile, expandedFragmentShaders, renderer, "Fragment", "fragment")
			writeSourceMap(outputSourceFile, expandedVertexShaders, renderer, "Vertex", "vertex")
		end
		
		outputSourceFile:close()
		
		print("Done.")
    end,
}

newoption
{
	trigger = "mingw",
	value = "VALUE",
	description = "MinGW variety",
	allowed =
	{
		{ "mingw", "MinGW or mingw32" },
		{ "mingw-pc", "MinGW-w64" },
		{ "mingw-w64", "MinGW-w64" }
	}
}

if _ACTION == nil then
	return
end

if not _OPTIONS["mingw"] then
	_OPTIONS["mingw"] = "mingw-w64"
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
