/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "Precompiled.h"
#pragma hdrstop

#include "../shaderc/shaderc.h"

extern const char *fallbackShader_bgfx_shader;
extern const char *fallbackShader_Common;
extern const char *fallbackShader_Defines;
extern const char *fallbackShader_Fog_fragment;
extern const char *fallbackShader_Fog_vertex;
extern const char *fallbackShader_Generators;
extern const char *fallbackShader_Generic_fragment;
extern const char *fallbackShader_Generic_vertex;
extern const char *fallbackShader_TextureColor_fragment;
extern const char *fallbackShader_TextureColor_vertex;
extern const char *fallbackShader_varying_def;

namespace renderer {

struct FallbackShader
{
	const char *filename;
	const char *source;
};

static const FallbackShader fallbackShaders[] =
{
	{ "shaders/bgfx_shader.sh", fallbackShader_bgfx_shader },
	{ "shaders/Common.sh", fallbackShader_Common },
	{ "shaders/Defines.sh", fallbackShader_Defines },
	{ "shaders/Fog_fragment.sc", fallbackShader_Fog_fragment },
	{ "shaders/Fog_vertex.sc", fallbackShader_Fog_vertex },
	{ "shaders/Generators.sh", fallbackShader_Generators },
	{ "shaders/Generic_fragment.sc", fallbackShader_Generic_fragment },
	{ "shaders/Generic_vertex.sc", fallbackShader_Generic_vertex },
	{ "shaders/TextureColor_fragment.sc", fallbackShader_TextureColor_fragment },
	{ "shaders/TextureColor_vertex.sc", fallbackShader_TextureColor_vertex },
	{ "shaders/varying_def.sc", fallbackShader_varying_def }
};

static const size_t nFallbackShaders = ARRAY_LEN(fallbackShaders);

static void ReleaseFile(const uint8_t* _ptr, void* /*_userData*/)
{
	delete [] _ptr;
}

struct CompileShaderCallback : public shaderc::CallbackI
{
	bool fileExists(const char* _filename)
	{
		if (ri.FS_ReadFile(_filename, NULL) != -1)
			return true;

		// If reading from a file fails, try an embedded fallback shader.
		for (size_t i = 0; i < nFallbackShaders; i++)
		{
			if (Q_stricmp(_filename, fallbackShaders[i].filename) == 0)
				return true;
		}

		return false;
	}

	shaderc::Memory readFile(const char* _filename)
	{
		void *buf;
		long size = ri.FS_ReadFile(_filename, &buf);
		shaderc::Memory mem;

		if (buf)
		{
			// Hunk temp memory needs to be freed in reverse order of alloc.
			auto bufCopy = new uint8_t[size];
			memcpy(bufCopy, buf, size);
			ri.FS_FreeFile(buf);

			mem.data = bufCopy;
			mem.size = size;
			mem.release = ReleaseFile;
		}
		else
		{
			// If reading from a file fails, try an embedded fallback shader.
			for (size_t i = 0; i < nFallbackShaders; i++)
			{
				auto &fs = fallbackShaders[i];

				if (Q_stricmp(_filename, fs.filename) == 0)
				{
					mem.data = (const uint8_t *)fs.source;
					mem.size = strlen(fs.source);
					mem.release = nullptr;
					break;
				}
			}
		}

		return mem;
	}

	bool writeFile(const char* _filename, const void* _data, int32_t _size)
	{
		mem = bgfx::copy(_data, _size);
		return true;
	}

	void writeLog(const char* _str) override
	{
		ri.Printf(PRINT_WARNING, "%s", _str);
	}

	const bgfx::Memory *mem;
};

static bgfx::ShaderHandle CreateShader(const char *name, shaderc::ShaderType::Enum type, const char *defines, size_t permutationIndex)
{
	const bgfx::RendererType::Enum backend = bgfx::getCaps()->rendererType;
	const char *backendName = nullptr;

	if (backend == bgfx::RendererType::OpenGL)
	{
		backendName = "gl";
	}
	else if (backend == bgfx::RendererType::Direct3D9)
	{
		backendName = "dx9";
	}
	else if (backend == bgfx::RendererType::Direct3D11)
	{
		backendName = "dx11";
	}
	
	// Compile shader.
	shaderc::Platform::Enum platform;
	shaderc::Profile::Enum profile;

	if (backend == bgfx::RendererType::OpenGL)
	{
		platform = shaderc::Platform::Linux;
		profile = shaderc::Profile::Enum(shaderc::Profile::GLSL_Version + 120);
	}
	else if (backend == bgfx::RendererType::Direct3D9)
	{
		platform = shaderc::Platform::Windows;
		profile = shaderc::Profile::HLSL3;
	}
	else if (backend == bgfx::RendererType::Direct3D11)
	{
		platform = shaderc::Platform::Windows;
		profile = shaderc::Profile::HLSL5;
	}

	const char *typeName = (type == shaderc::ShaderType::Vertex ? "vertex" : "fragment");
	const std::string inputFilePath(va("shaders/%s_%s.sc", name, typeName));
	const std::string varingdefPath("shaders/varying_def.sc");

	shaderc::CompileShaderParameters params(type, platform);
	params.profile = profile;
	params.inputFilePath = inputFilePath.c_str();
	params.varingdefPath = varingdefPath.c_str();
	params.preprocessorDefines = defines;
	CompileShaderCallback callback;
	params.callback = &callback;
	
	if (!shaderc::compileShader(params))
	{
		ri.Printf(PRINT_WARNING, "Error compiling shader %s\n", params.inputFilePath);
		return BGFX_INVALID_HANDLE;
	}

	return bgfx::createShader(callback.mem);
}

void ShaderCache::initialize()
{
	createBundle(&fog_, "Fog", nullptr, nullptr);
	char defines[2048];
	#define DEFINE(x) { Q_strcat(defines, sizeof(defines), x); Q_strcat(defines, sizeof(defines), ";"); }

	for (size_t i = 0; i < GenericPermutations::Count; i++)
	{
		if (i & GenericPermutations::AlphaTest)
		{
			DEFINE("USE_ALPHA_TEST");
		}

		// Stop if compiling one of the permutations fails. Don't want to flood the console with too many error messages.
		if (!createBundle(&generic_[i], "Generic", defines, defines, i, i))
			break;
	}

	if (!createBundle(&textureColor_, "TextureColor", nullptr, nullptr))
	{
		ri.Error(ERR_FATAL, "A valid TextureColor shader is required");
	}
	#undef DEFINE
}

bgfx::ProgramHandle ShaderCache::getHandle(ShaderProgramId program, int programIndex, int flags) const
{
	bgfx::ProgramHandle handle = BGFX_INVALID_HANDLE;

	if (program == ShaderProgramId::Fog)
	{
		handle = fog_.program.handle;
	}
	else if (program == ShaderProgramId::Generic)
	{
		handle = generic_[programIndex].program.handle;
	}
	else if (program == ShaderProgramId::TextureColor)
	{
		handle = textureColor_.program.handle;
	}

	if (bgfx::isValid(handle) || (flags & GetHandleFlags::ReturnInvalid))
		return handle;
	
	// Fallback to TextureColor shader program.
	return textureColor_.program.handle;
}

bool ShaderCache::createBundle(Bundle *bundle, const char *name, const char *vertexDefines, const char *fragmentDefines, size_t vertexPermutationIndex, size_t fragmentPermutationIndex)
{
	assert(bundle);
	assert(name && name[0]);
	bundle->vertex.handle = CreateShader(name, shaderc::ShaderType::Vertex, vertexDefines, fragmentPermutationIndex);

	if (!bgfx::isValid(bundle->vertex.handle))
		return false;

	bundle->fragment.handle = CreateShader(name, shaderc::ShaderType::Fragment, fragmentDefines, vertexPermutationIndex);

	if (!bgfx::isValid(bundle->fragment.handle))
		return false;

	bundle->program.handle = bgfx::createProgram(bundle->vertex.handle, bundle->fragment.handle);
	return bgfx::isValid(bundle->program.handle);
}

} // namespace renderer
