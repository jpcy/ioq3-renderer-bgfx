local globalOutputPath = "build/"
local renderers = nil
local tempInputFilename = globalOutputPath .. "tempinput"
local tempOutputFilename = globalOutputPath .. "tempoutput"
local outputFilename = globalOutputPath .. "Shaders.h"

if os.is("windows") then
	renderers = { "gl", "d3d9", "d3d11" }
else
	renderers = { "gl" }
end

function initShaderCompilation()
	-- Delete output file so we can append to it.
	os.remove(outputFilename)
end

function compileShader(bgfxPath, input, type, permutation, defines)
	io.write("Compiling " .. input .. "_" .. type)
	
	if permutation == nil then
		io.write("\n")
	else
		io.write(" " .. permutation .. "\n")
	end
	
	local inputFilename = "shaders/" .. input .. "_" .. type .. ".sc"
	
	-- Handle inserting defines.
	if defines ~= nil then
		-- Write the defines and the input file data to a temp file.
		local inputFile = io.open(inputFilename, "r")
		local tempFile = io.open(tempInputFilename, "w")
		local definesInserted = false
		
		while true do
			inputFileLine = inputFile:read()
			
			if inputFileLine == nil then
				break
			end
			
			-- The defines are inserted before the first line that doesn't start with $, since the varyings must be first.
			if not definesInserted and inputFileLine:find("^$") then
				tempFile:write(defines .. "\n")
				definesInserted = true
			end
			
			tempFile:write(inputFileLine .. "\n")
		end
		
		inputFile:close()
		tempFile:close()
		
		-- The temp filename will be used as shaderc input.
		inputFilename = tempInputFilename
	end
	
	-- Compile the shader for all renderers.
	for _,renderer in pairs(renderers) do
		local command = nil
		
		if os.is("windows") then
			command = "shaderc.exe"
		elseif os.is("linux") and not os.is64bit() then
			command = "`./shaderc32"
		elseif os.is("linux") and os.is64bit() then
			command = "`./shaderc64"
		end
		
		local variableName = input .. "_"

		if permutation ~= nil then
			variableName = variableName .. permutation .. "_"
		end

		variableName = variableName .. type .. "_" .. renderer
		
		command = command .. " -i \"shaders;" .. path.join(bgfxPath, "src") .. "\" -f \"" .. inputFilename .. "\" -o \"" .. tempOutputFilename .. "\" --varyingdef shaders/varying.def.sc --bin2c \"" .. variableName .. "\" --type " .. type
	
		if renderer == "gl" then
			command = command .. " --platform linux"
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
		local outputFile = io.open(outputFilename, "a")
		outputFile:write("\n")
		outputFile:write(tempContent)
		outputFile:close()
	end
end
