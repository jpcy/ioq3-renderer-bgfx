local globalOutputPath = "build/shaders/"
local renderers = nil
local tempFilename = globalOutputPath .. "temp"

if os.get() == "windows" then
	renderers = { "gl", "d3d9", "d3d11" }
else
	renderers = { "gl" }
end

function compileShader(input, type, permutation, defines)
	io.write("Compiling " .. input .. "_" .. type)
	
	if permutation == nil then
		io.write("\n")
	else
		io.write(" " .. permutation .. "\n")
	end
	
	local inputFilename = "shaders/" .. input .. "_" .. type .. ".sc"
	
	-- Handle inserting defines.
	if defines ~= nil then
		local inputFile = io.open(inputFilename, "r")
		local inputFileLine = inputFile:read()
		
		if inputFileLine == nil then
			print("File is empty")
			return
		end
		
		-- Write the defines and the input file data to a temp file.
		-- The defines are inserted as the second line, since the varyings must be first.
		local tempFile = io.open(tempFilename, "w")
		tempFile:write(inputFileLine .. "\n")
		tempFile:write(defines .. "\n")
		
		while true do
			inputFileLine = inputFile:read()
			
			if inputFileLine == nil then
				break
			end
			
			tempFile:write(inputFileLine .. "\n")
		end
		
		inputFile:close()
		tempFile:close()
		
		-- The temp filename will be used as shaderc input.
		inputFilename = tempFilename
	end
	
	-- Compile the shader for all renderers.
	for _,renderer in pairs(renderers) do
		local outputName = input .. "_"

		if permutation ~= nil then
			outputName = outputName .. permutation .. "_"
		end

		outputName = outputName .. type .. "_" .. renderer
		local command = nil
		
		if os.is("windows") then
			command = "shaderc.exe"
		elseif os.is("linux") and not os.is64bit() then
			command = "`./shaderc32"
		elseif os.is("linux") and os.is64bit() then
			command = "`./shaderc64"
		end
		
		command = command .. " -i shaders -f \"" .. inputFilename .. "\" -o \"" .. globalOutputPath .. outputName .. ".h\" --varyingdef shaders/varying.def.sc --bin2c \"" .. outputName .. "\" --type " .. type
	
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
		
		os.execute(command)
	end
end
