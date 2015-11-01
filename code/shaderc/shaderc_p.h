/*
 * Copyright 2011-2015 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef SHADERC_P_H_HEADER_GUARD
#define SHADERC_P_H_HEADER_GUARD

#define _BX_TRACE(_format, ...) \
				BX_MACRO_BLOCK_BEGIN \
					if (g_verbose) \
					{ \
						fprintf(stderr, BX_FILE_LINE_LITERAL "" _format "\n", ##__VA_ARGS__); \
					} \
				BX_MACRO_BLOCK_END

#define _BX_WARN(_condition, _format, ...) \
				BX_MACRO_BLOCK_BEGIN \
					if (!(_condition) ) \
					{ \
						BX_TRACE("WARN " _format, ##__VA_ARGS__); \
					} \
				BX_MACRO_BLOCK_END

#define _BX_CHECK(_condition, _format, ...) \
				BX_MACRO_BLOCK_BEGIN \
					if (!(_condition) ) \
					{ \
						BX_TRACE("CHECK " _format, ##__VA_ARGS__); \
						bx::debugBreak(); \
					} \
				BX_MACRO_BLOCK_END

#define BX_TRACE _BX_TRACE
#define BX_WARN  _BX_WARN
#define BX_CHECK _BX_CHECK

#ifndef SHADERC_CONFIG_HLSL
#	define SHADERC_CONFIG_HLSL BX_PLATFORM_WINDOWS
#endif // SHADERC_CONFIG_HLSL

#include <alloca.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

#include <bx/bx.h>
#include <bx/debug.h>
#include <bx/commandline.h>
#include <bx/endian.h>
#include <bx/uint32_t.h>
#include <bx/readerwriter.h>
#include <bx/string.h>
#include <bx/hash.h>
#include "../../src/vertexdecl.h"
#include "shaderc.h"

#define BGFX_UNIFORM_FRAGMENTBIT UINT8_C(0x10)

namespace shaderc
{
	extern bool g_verbose;

	class LineReader
	{
	public:
		LineReader(const char* _str)
			: m_str(_str)
			, m_pos(0)
			, m_size((uint32_t)strlen(_str))
		{
		}

		std::string getLine()
		{
			const char* str = &m_str[m_pos];
			skipLine();

			const char* eol = &m_str[m_pos];

			std::string tmp;
			tmp.assign(str, eol - str);
			return tmp;
		}

		bool isEof() const
		{
			return m_str[m_pos] == '\0';
		}

		void skipLine()
		{
			const char* str = &m_str[m_pos];
			const char* nl = bx::strnl(str);
			m_pos += (uint32_t)(nl - str);
		}

		const char* m_str;
		uint32_t m_pos;
		uint32_t m_size;
	};

	class FileWriter
	{
	public:
		FileWriter(CallbackI* callback);
		const uint8_t* getData() const { return m_data; }
		int32_t getSize() const { return m_size; }
		int32_t open(const char* _filePath);
		int32_t close();
		int32_t write(const void* _data, int32_t _size);
		int32_t writef(const char* _format, ...);

		template<typename Ty>
		inline int32_t write(const Ty& _value)
		{
			BX_STATIC_ASSERT(BX_TYPE_IS_POD(Ty) );
			return write(&_value, sizeof(Ty) );
		}

	private:
		CallbackI* m_callback;
		std::string m_filePath;
		bx::CrtFileWriter m_fileWriter;
		bx::CrtAllocator m_allocator;
		bx::MemoryBlock m_memBlock;
		uint8_t* m_data;
		int32_t m_pos;
		int32_t m_top;
		int32_t m_size;
	};

	struct UniformType
	{
		enum Enum
		{
			Int1,
			End,

			Vec4,
			Mat3,
			Mat4,

			Count
		};
	};

	const char* getUniformTypeName(UniformType::Enum _enum);
	UniformType::Enum nameToUniformTypeEnum(const char* _name);

	struct Uniform
	{
		std::string name;
		UniformType::Enum type;
		uint8_t num;
		uint16_t regIndex;
		uint16_t regCount;
	};

	typedef std::vector<Uniform> UniformArray;

	void writeLog(CallbackI* _callback, const char* _format, ...);
	void printCode(CallbackI* _callback, const char* _code, int32_t _line = 0, int32_t _start = 0, int32_t _end = INT32_MAX);
	void strreplace(char* _str, const char* _find, const char* _replace);
	bool writeFile(CallbackI* _callback, const char* _filePath, const void* _data, int32_t _size);

	bool compileHLSLShader(const CompileShaderParameters& _params, uint32_t _d3d, const std::string& _code, FileWriter* _writer, bool firstPass = true);
	bool compileGLSLShader(const CompileShaderParameters& _params, uint32_t _gles, const std::string& _code, FileWriter* _writer);
} // namespace shaderc

#endif // SHADERC_P_H_HEADER_GUARD
