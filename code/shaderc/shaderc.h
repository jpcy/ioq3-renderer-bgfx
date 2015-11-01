/*
 * Copyright 2011-2015 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef SHADERC_H_HEADER_GUARD
#define SHADERC_H_HEADER_GUARD

#include <bgfx/bgfx.h>

#define SHADERC_BIT(x) (1<<(x))

namespace shaderc
{
	struct ShaderType
	{
		enum Enum
		{
			Compute,
			Fragment,
			Vertex
		};
	};

	struct Platform
	{
		enum Enum
		{
			Android,
			asm_js,
			iOS,
			Linux,
			NaCl,
			OSX,
			Windows,
			Xbox360
		};
	};

	struct Profile
	{
		enum Enum
		{
			Unspecified,
			ESSL,
			HLSL3,
			HLSL4,
			HLSL4_1,
			HLSL4_Level9_1,
			HLSL4_Level9_3,
			HLSL5,
			Metal,
			GLSL_Version // e.g. GLSL_Version + 330
		};
	};

	struct Flags
	{
		enum
		{
			Depends                   = SHADERC_BIT(1),
			PreprocessOnly            = SHADERC_BIT(2),
			Raw                       = SHADERC_BIT(3),
			Verbose                   = SHADERC_BIT(4),
			D3DAvoidFlowControl       = SHADERC_BIT(5),
			D3DBackwardsCompatibility = SHADERC_BIT(6),
			D3DDebug                  = SHADERC_BIT(7),
			D3DDisassemble            = SHADERC_BIT(8),
			D3DNoPreshader            = SHADERC_BIT(9),
			D3DOptimize               = SHADERC_BIT(10),
			D3DPartialPrecision       = SHADERC_BIT(11),
			D3DPreferFlowControl      = SHADERC_BIT(12),
			D3DWarningsAsErrors       = SHADERC_BIT(13)
		};
	};

	typedef void (*ReleaseFn)(const uint8_t* _ptr, void* _userData);

	void defaultRelease(const uint8_t* _ptr, void* _userData);

	struct Memory
	{
		const uint8_t* data;
		uint32_t size;
		ReleaseFn release;
		void* userData;

		Memory()
			: data(NULL)
			, size(0)
			, release(defaultRelease)
			, userData(NULL)
		{
		}
	};

	struct CallbackI
	{
		virtual ~CallbackI() = 0;
		virtual bool fileExists(const char* _filename) = 0;
		virtual Memory readFile(const char* _filename) = 0;
		virtual bool writeFile(const char* _filename, const void* _data, int32_t _size) = 0;
		virtual void writeLog(const char* _str) = 0;
	};

	inline CallbackI::~CallbackI()
	{
	}

	struct CompileShaderParameters
	{
		ShaderType::Enum shaderType;
		Platform::Enum platform;
		Profile::Enum profile;
		const char* inputFilePath;
		const char* includePaths;
		const char* outputFilePath;
		const char* bin2cFilePath;
		const char* varingdefPath;
		const char* preprocessorDefines;
		CallbackI *callback;
		uint32_t flags;
		uint32_t d3dOptimizationLevel;

		CompileShaderParameters(ShaderType::Enum _shaderType, Platform::Enum _platform)
			: shaderType(_shaderType)
			, platform(_platform)
			, profile(Profile::Unspecified)
			, inputFilePath(NULL)
			, includePaths(NULL)
			, outputFilePath(NULL)
			, bin2cFilePath(NULL)
			, varingdefPath(NULL)
			, preprocessorDefines(NULL)
			, callback(NULL)
			, flags(Flags::D3DOptimize)
			, d3dOptimizationLevel(3)
		{}
	};

	bool compileShader(const CompileShaderParameters& _params);
} // namespace shaderc

#endif // SHADERC_H_HEADER_GUARD
