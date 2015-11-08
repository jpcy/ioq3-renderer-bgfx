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

extern "C"
{
	extern const char *fallbackShader_bgfx_shader;
	extern const char *fallbackShader_Common;
	extern const char *fallbackShader_Fog_fragment;
	extern const char *fallbackShader_Fog_vertex;
	extern const char *fallbackShader_Generators;
	extern const char *fallbackShader_Generic_fragment;
	extern const char *fallbackShader_Generic_vertex;
	extern const char *fallbackShader_Lighting;
	extern const char *fallbackShader_Lit_fragment;
	extern const char *fallbackShader_Lit_vertex;
	extern const char *fallbackShader_TextureColor_fragment;
	extern const char *fallbackShader_TextureColor_vertex;
	extern const char *fallbackShader_varying_def;
}

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
	{ "shaders/Fog_fragment.sc", fallbackShader_Fog_fragment },
	{ "shaders/Fog_vertex.sc", fallbackShader_Fog_vertex },
	{ "shaders/Generators.sh", fallbackShader_Generators },
	{ "shaders/Generic_fragment.sc", fallbackShader_Generic_fragment },
	{ "shaders/Generic_vertex.sc", fallbackShader_Generic_vertex },
	{ "shaders/Lighting.sh", fallbackShader_Lighting },
	{ "shaders/Lit_fragment.sc", fallbackShader_Lit_fragment },
	{ "shaders/Lit_vertex.sc", fallbackShader_Lit_vertex },
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
		ri.Printf(PRINT_WARNING, _str);
	}

	const bgfx::Memory *mem;
};

static bgfx::ShaderHandle CreateShader(const char *name, shaderc::ShaderType::Enum type, const char *defines = nullptr, size_t permutationIndex = 0)
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
		ri.Printf(PRINT_ERROR, "Error compiling shader %s\n", params.inputFilePath);
		return BGFX_INVALID_HANDLE;
	}

	return bgfx::createShader(callback.mem);
}

ShaderCache::ShaderCache()
{
	constants_[0] = 0;
	#define CONSTANT(format, ...) { char temp[256]; Com_sprintf(temp, sizeof(temp), format, ##__VA_ARGS__); Q_strcat(constants_, sizeof(constants_), temp); }
	CONSTANT("AGEN_IDENTITY=%d;", MaterialAlphaGen::Identity);
	CONSTANT("AGEN_LIGHTING_SPECULAR=%d;", MaterialAlphaGen::LightingSpecular);
	CONSTANT("AGEN_PORTAL=%d;", MaterialAlphaGen::Portal);
	CONSTANT("CGEN_IDENTITY=%d;", MaterialColorGen::Identity);
	CONSTANT("CGEN_LIGHTING_DIFFUSE=%d;", MaterialColorGen::LightingDiffuse);
	CONSTANT("DEFORM1_AMPLITUDE=%d;", Uniforms::DeformParameters1::Amplitude);
	CONSTANT("DEFORM1_BASE=%d;", Uniforms::DeformParameters1::Base);
	CONSTANT("DEFORM1_FREQUENCY=%d;", Uniforms::DeformParameters1::Frequency);
	CONSTANT("DEFORM1_PHASE=%d;", Uniforms::DeformParameters1::Phase);
	CONSTANT("DEFORM2_SPREAD=%d;", Uniforms::DeformParameters2::Spread);
	CONSTANT("DEFORM2_TIME=%d;", Uniforms::DeformParameters2::Time);
	CONSTANT("DGEN_NONE=%d;", MaterialDeformGen::None);
	CONSTANT("DGEN_WAVE_SIN=%d;", MaterialDeformGen::Sin);
	CONSTANT("DGEN_WAVE_SQUARE=%d;", MaterialDeformGen::Square);
	CONSTANT("DGEN_WAVE_TRIANGLE=%d;", MaterialDeformGen::Triangle);
	CONSTANT("DGEN_WAVE_SAWTOOTH=%d;", MaterialDeformGen::Sawtooth);
	CONSTANT("DGEN_WAVE_INVERSE_SAWTOOTH=%d;", MaterialDeformGen::InverseSawtooth);
	CONSTANT("DGEN_BULGE=%d;", MaterialDeformGen::Bulge);
	CONSTANT("DGEN_MOVE=%d;", MaterialDeformGen::Move);
	CONSTANT("GEN_ALPHA=%d;", Uniforms::Generators::Alpha);
	CONSTANT("GEN_COLOR=%d;", Uniforms::Generators::Color);
	CONSTANT("GEN_DEFORM=%d;", Uniforms::Generators::Deform);
	CONSTANT("GEN_TEXCOORD=%d;", Uniforms::Generators::TexCoord);
	CONSTANT("TCGEN_NONE=%d;", MaterialTexCoordGen::None);
	CONSTANT("TCGEN_ENVIRONMENT_MAPPED=%d;", MaterialTexCoordGen::EnvironmentMapped);
	CONSTANT("TCGEN_FOG=%d;", MaterialTexCoordGen::Fog);
	CONSTANT("TCGEN_LIGHTMAP=%d;", MaterialTexCoordGen::Lightmap);
	CONSTANT("TCGEN_TEXTURE=%d;", MaterialTexCoordGen::Texture);
	CONSTANT("TCGEN_VECTOR=%d;", MaterialTexCoordGen::Vector);
	CONSTANT("ATEST_GT_0=%d;", MaterialAlphaTest::GT_0);
	CONSTANT("ATEST_LT_128=%d;", MaterialAlphaTest::LT_128);
	CONSTANT("ATEST_GE_128=%d;", MaterialAlphaTest::GE_128);
	CONSTANT("LIGHT_NONE=%d;", MaterialLight::None);
	CONSTANT("LIGHT_MAP=%d;", MaterialLight::Map);
	CONSTANT("LIGHT_VERTEX=%d;", MaterialLight::Vertex);
	CONSTANT("LIGHT_VECTOR=%d;", MaterialLight::Vector);
	#undef CONSTANT
}

void ShaderCache::initialize()
{
	createBundle(&fog_, "Fog", constants_, nullptr);
	char defines[2048];
	#define DEFINE(x) Q_strcat(defines, sizeof(defines), x); Q_strcat(defines, sizeof(defines), ";");

	for (size_t i = 0; i < GenericPermutations::Count; i++)
	{
		Q_strncpyz(defines, constants_, sizeof(defines));

		if (i & GenericPermutations::AlphaTest)
		{
			DEFINE("USE_ALPHA_TEST");
		}

		// Stop if compiling one of the permutations fails. Don't want to flood the console with too many error messages.
		if (!createBundle(&generic_[i], "Generic", defines, defines, i, i))
			break;
	}

	for (size_t i = 0; i < LitPermutations::Count; i++)
	{
		Q_strncpyz(defines, constants_, sizeof(defines));

		if (g_main->cvars.deluxeSpecular->value > 0.000001f)
			DEFINE(va("r_deluxeSpecular=%f;", g_main->cvars.deluxeSpecular->value));

		if (g_main->cvars.specularIsMetallic->value)
			DEFINE("SPECULAR_IS_METALLIC");

		if (1)
			DEFINE("SWIZZLE_NORMALMAP");

		const bool phong = g_main->cvars.normalMapping->integer || g_main->cvars.specularMapping->integer;

		if (phong)
			DEFINE("USE_PHONG_SHADING");

		if (g_main->cvars.deluxeMapping->integer && phong)
			DEFINE("USE_DELUXEMAP");

		if (g_main->cvars.normalMapping->integer)
		{
			DEFINE("USE_NORMALMAP");

			if (g_main->cvars.normalMapping->integer == 2)
				DEFINE("USE_OREN_NAYAR");

			if (g_main->cvars.normalMapping->integer == 3)
				DEFINE("USE_TRIACE_OREN_NAYAR");

#ifdef USE_VERT_TANGENT_SPACE
			DEFINE("USE_VERT_TANGENT_SPACE");
#endif
		}

		if (g_main->cvars.specularMapping->integer)
		{
			DEFINE("USE_SPECULARMAP");

			switch (g_main->cvars.specularMapping->integer)
			{
			case 1:
			default:
				DEFINE("USE_BLINN");
				break;

			case 2:
				DEFINE("USE_BLINN_FRESNEL");
				break;

			case 3:
				DEFINE("USE_MCAULEY");
				break;

			case 4:
				DEFINE("USE_GOTANDA");
				break;

			case 5:
				DEFINE("USE_LAZAROV");
				break;
			}
		}

		if (i & LitPermutations::AlphaTest)
		{
			DEFINE("USE_ALPHA_TEST");
		}

		// Stop if compiling one of the permutations fails. Don't want to flood the console with too many error messages.
		if (!createBundle(&lit_[i], "Lit", defines, defines, i, i))
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
	else if (program == ShaderProgramId::Lit)
	{
		handle = lit_[programIndex].program.handle;
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
