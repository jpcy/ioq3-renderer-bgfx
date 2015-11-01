/*
 * Copyright 2011-2015 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include <assert.h>
#include "shaderc_p.h"
#include "shaderc.h"

#define MAX_TAGS 256
extern "C"
{
#include <fpp.h>
} // extern "C"

#define BGFX_CHUNK_MAGIC_CSH BX_MAKEFOURCC('C', 'S', 'H', 0x2)
#define BGFX_CHUNK_MAGIC_FSH BX_MAKEFOURCC('F', 'S', 'H', 0x4)
#define BGFX_CHUNK_MAGIC_VSH BX_MAKEFOURCC('V', 'S', 'H', 0x4)

namespace shaderc
{
	bool g_verbose = false;

	long int fsize(FILE* _file)
	{
		long int pos = ftell(_file);
		fseek(_file, 0L, SEEK_END);
		long int size = ftell(_file);
		fseek(_file, pos, SEEK_SET);
		return size;
	}

	static const char* s_ARB_shader_texture_lod[] =
	{
		"texture2DLod",
		"texture2DProjLod",
		"texture3DLod",
		"texture3DProjLod",
		"textureCubeLod",
		"shadow2DLod",
		"shadow2DProjLod",
		NULL
		// "texture1DLod",
		// "texture1DProjLod",
		// "shadow1DLod",
		// "shadow1DProjLod",
	};

	static const char* s_EXT_shadow_samplers[] =
	{
		"shadow2D",
		"shadow2DProj",
		"sampler2DShadow",
		NULL
	};

	static const char* s_OES_standard_derivatives[] =
	{
		"dFdx",
		"dFdy",
		"fwidth",
		NULL
	};

	static const char* s_OES_texture_3D[] =
	{
		"texture3D",
		"texture3DProj",
		"texture3DLod",
		"texture3DProjLod",
		NULL
	};

	const char* s_uniformTypeName[UniformType::Count] =
	{
		"int",
		NULL,
		"vec4",
		"mat3",
		"mat4",
	};

	const char* interpolationDx11(const char* _glsl)
	{
		if (0 == strcmp(_glsl, "smooth") )
		{
			return "linear";
		}
		else if (0 == strcmp(_glsl, "flat") )
		{
			return "nointerpolation";
		}

		return _glsl; // noperspective
	}

	const char* getUniformTypeName(UniformType::Enum _enum)
	{
		return s_uniformTypeName[_enum];
	}

	UniformType::Enum nameToUniformTypeEnum(const char* _name)
	{
		for (uint32_t ii = 0; ii < UniformType::Count; ++ii)
		{
			if (NULL != s_uniformTypeName[ii]
			&&  0 == strcmp(_name, s_uniformTypeName[ii]) )
			{
				return UniformType::Enum(ii);
			}
		}

		return UniformType::Count;
	}

	class Bin2cWriter : public bx::CrtFileWriter
	{
	public:
		Bin2cWriter(const char* _name)
			: m_name(_name)
		{
		}

		virtual ~Bin2cWriter()
		{
		}

		virtual int32_t close() BX_OVERRIDE
		{
			generate();
			return bx::CrtFileWriter::close();
		}

		virtual int32_t write(const void* _data, int32_t _size) BX_OVERRIDE
		{
			const char* data = (const char*)_data;
			m_buffer.insert(m_buffer.end(), data, data+_size);
			return _size;
		}

	private:
		void generate()
		{
	#define HEX_DUMP_WIDTH 16
	#define HEX_DUMP_SPACE_WIDTH 96
	#define HEX_DUMP_FORMAT "%-" BX_STRINGIZE(HEX_DUMP_SPACE_WIDTH) "." BX_STRINGIZE(HEX_DUMP_SPACE_WIDTH) "s"
			const uint8_t* data = &m_buffer[0];
			uint32_t size = (uint32_t)m_buffer.size();

			outf("static const uint8_t %s[%d] =\n{\n", m_name.c_str(), size);

			if (NULL != data)
			{
				char hex[HEX_DUMP_SPACE_WIDTH+1];
				char ascii[HEX_DUMP_WIDTH+1];
				uint32_t hexPos = 0;
				uint32_t asciiPos = 0;
				for (uint32_t ii = 0; ii < size; ++ii)
				{
					bx::snprintf(&hex[hexPos], sizeof(hex)-hexPos, "0x%02x, ", data[asciiPos]);
					hexPos += 6;

					ascii[asciiPos] = isprint(data[asciiPos]) && data[asciiPos] != '\\' ? data[asciiPos] : '.';
					asciiPos++;

					if (HEX_DUMP_WIDTH == asciiPos)
					{
						ascii[asciiPos] = '\0';
						outf("\t" HEX_DUMP_FORMAT "// %s\n", hex, ascii);
						data += asciiPos;
						hexPos = 0;
						asciiPos = 0;
					}
				}

				if (0 != asciiPos)
				{
					ascii[asciiPos] = '\0';
					outf("\t" HEX_DUMP_FORMAT "// %s\n", hex, ascii);
				}
			}

			outf("};\n");
	#undef HEX_DUMP_WIDTH
	#undef HEX_DUMP_SPACE_WIDTH
	#undef HEX_DUMP_FORMAT
		}

		int32_t outf(const char* _format, ...)
		{
			va_list argList;
			va_start(argList, _format);

			char temp[2048];
			char* out = temp;
			int32_t max = sizeof(temp);
			int32_t len = bx::vsnprintf(out, max, _format, argList);
			if (len > max)
			{
				out = (char*)alloca(len);
				len = bx::vsnprintf(out, len, _format, argList);
			}

			int32_t size = bx::CrtFileWriter::write(out, len);

			va_end(argList);

			return size;
		}

		std::string m_filePath;
		std::string m_name;
		typedef std::vector<uint8_t> Buffer;
		Buffer m_buffer;
	};

	struct Varying
	{
		std::string m_precision;
		std::string m_interpolation;
		std::string m_name;
		std::string m_type;
		std::string m_init;
		std::string m_semantics;
	};

	typedef std::unordered_map<std::string, Varying> VaryingMap;

	FileWriter::FileWriter(CallbackI* _callback)
		: m_callback(_callback)
		, m_memBlock(&m_allocator)
		, m_data(NULL)
		, m_pos(0)
		, m_top(0)
		, m_size(0)
	{
	}

	int32_t FileWriter::open(const char* _filePath)
	{
		if (NULL == m_callback)
		{
			return m_fileWriter.open(_filePath, false);
		}

		if (NULL != _filePath)
		{
			m_filePath = _filePath;
		}

		return 0;
	}

	int32_t FileWriter::close()
	{
		if (NULL == m_callback)
		{
			return m_fileWriter.close();
		}

		if (!m_callback->writeFile(m_filePath.c_str(), m_data, m_size))
		{
			return 1;
		}

		return 0;
	}

	int32_t FileWriter::write(const void* _data, int32_t _size)
	{
		if (NULL == m_callback)
		{
			return m_fileWriter.write(_data, _size);
		}

		int32_t morecore = int32_t(m_pos - m_size) + _size;

		if (0 < morecore)
		{
			morecore = BX_ALIGN_MASK(morecore, 0xfff);
			m_data = (uint8_t*)m_memBlock.more(morecore);
			m_size = m_memBlock.getSize();
		}

		int32_t reminder = m_size-m_pos;
		int32_t size = bx::uint32_min(_size, int32_t(reminder > INT32_MAX ? INT32_MAX : reminder) );
		memcpy(&m_data[m_pos], _data, size);
		m_pos += size;
		m_top = std::max(m_top, m_pos);
		return size;
	}

	int32_t FileWriter::writef(const char* _format, ...)
	{
		va_list argList;
		va_start(argList, _format);

		char temp[2048];

		char* out = temp;
		int32_t max = sizeof(temp);
		int32_t len = bx::vsnprintf(out, max, _format, argList);
		if (len > max)
		{
			out = (char*)alloca(len);
			len = bx::vsnprintf(out, len, _format, argList);
		}

		len = write(out, len);

		va_end(argList);

		return len;
	}

	void strins(char* _str, const char* _insert)
	{
		size_t len = strlen(_insert);
		memmove(&_str[len], _str, strlen(_str) );
		memcpy(_str, _insert, len);
	}

	void strreplace(char* _str, const char* _find, const char* _replace)
	{
		const size_t len = strlen(_find);

		char* replace = (char*)alloca(len+1);
		bx::strlcpy(replace, _replace, len+1);
		for (size_t ii = strlen(replace); ii < len; ++ii)
		{
			replace[ii] = ' ';
		}
		replace[len] = '\0';

		BX_CHECK(len >= strlen(_replace), "");
		for (char* ptr = strstr(_str, _find); NULL != ptr; ptr = strstr(ptr + len, _find) )
		{
			memcpy(ptr, replace, len);
		}
	}

	void defaultRelease(const uint8_t* _ptr, void* /*_userData*/)
	{
		delete [] _ptr;
	}

	static void release(Memory mem)
	{
		if (NULL != mem.data && NULL != mem.release)
		{
			mem.release(mem.data, mem.userData);
		}
	}

	static void writeLog(CallbackI* _callback, const char* _format, va_list _vargs)
	{
		if (_callback)
		{
			char temp[8192];
			bx::vsnprintf(temp, sizeof(temp), _format, _vargs);
			va_end(_vargs);
			temp[sizeof(temp)-1] = '\0';
			_callback->writeLog(temp);
		}
	}

	void writeLog(CallbackI* _callback, const char* _format, ...)
	{
		if (_callback)
		{
			va_list argList;
			va_start(argList, _format);
			writeLog(_callback, _format, argList);
		}
	}

	void printCode(CallbackI* _callback, const char* _code, int32_t _line, int32_t _start, int32_t _end)
	{
		writeLog(_callback, "Code:\n---\n");

		LineReader lr(_code);
		for (int32_t line = 1; !lr.isEof() && line < _end; ++line)
		{
			if (line >= _start)
			{
				writeLog(_callback, "%s%3d: %s", _line == line ? ">>> " : "    ", line, lr.getLine().c_str() );
			}
			else
			{
				lr.skipLine();
			}
		}

		writeLog(_callback, "---\n");
	}

	bool writeFile(CallbackI* _callback, const char* _filePath, const void* _data, int32_t _size)
	{
		if (NULL == _callback)
		{
			bx::CrtFileWriter out;
			if (0 == out.open(_filePath) )
			{
				out.write(_data, _size);
				out.close();
				return true;
			}

			return false;
		}

		return _callback->writeFile(_filePath, _data, _size);
	}

	struct Preprocessor
	{
		Preprocessor(const char* _filePath, bool _gles, const char* _includeDir, CallbackI* _callback)
			: m_tagptr(m_tags)
			, m_scratchPos(0)
			, m_callback(_callback)
		{
			m_tagptr->tag = FPPTAG_USERDATA;
			m_tagptr->data = this;
			m_tagptr++;

			m_tagptr->tag = FPPTAG_DEPENDS;
			m_tagptr->data = (void*)fppDepends;
			m_tagptr++;

			m_tagptr->tag = FPPTAG_FILE_EXISTS;
			m_tagptr->data = (void*)fppFileExists;
			m_tagptr++;

			m_tagptr->tag = FPPTAG_INPUT;
			m_tagptr->data = (void*)fppInput;
			m_tagptr++;

			m_tagptr->tag = FPPTAG_OUTPUT;
			m_tagptr->data = (void*)fppOutput;
			m_tagptr++;

			m_tagptr->tag = FPPTAG_ERROR;
			m_tagptr->data = (void*)fppError;
			m_tagptr++;

			m_tagptr->tag = FPPTAG_IGNOREVERSION;
			m_tagptr->data = (void*)0;
			m_tagptr++;

			m_tagptr->tag = FPPTAG_LINE;
			m_tagptr->data = (void*)0;
			m_tagptr++;

			m_tagptr->tag = FPPTAG_INPUT_NAME;
			m_tagptr->data = m_filePath = scratch(_filePath);
			m_tagptr++;

			if (NULL != _includeDir)
			{
				addInclude(_includeDir);
			}

			if (!_gles)
			{
				m_default = "#define lowp\n#define mediump\n#define highp\n";
			}
		}

		void setDefine(const char* _define)
		{
			m_tagptr->tag = FPPTAG_DEFINE;
			m_tagptr->data = scratch(_define);
			m_tagptr++;
		}

		void setDefines(const char* _defines)
		{
			char* start = scratch(_defines);

			for (char* split = strchr(start, ';'); NULL != split; split = strchr(start, ';'))
			{
				*split = '\0';
				m_tagptr->tag = FPPTAG_DEFINE;
				m_tagptr->data = start;
				m_tagptr++;
				start = split + 1;
			}

			if (start && start[0])
			{
				m_tagptr->tag = FPPTAG_DEFINE;
				m_tagptr->data = start;
				m_tagptr++;
			}
		}

		void setDefaultDefine(const char* _name)
		{
			char temp[1024];
			bx::snprintf(temp, BX_COUNTOF(temp)
				, "#ifndef %s\n"
				  "#	define %s 0\n"
				  "#endif // %s\n"
				  "\n"
				, _name
				, _name
				, _name
				);

			m_default += temp;
		}

		void writef(const char* _format, ...)
		{
			va_list argList;
			va_start(argList, _format);
			bx::stringPrintfVargs(m_default, _format, argList);
			va_end(argList);
		}

		void addInclude(const char* _includeDir)
		{
			char* start = scratch(_includeDir);

			for (char* split = strchr(start, ';'); NULL != split; split = strchr(start, ';'))
			{
				*split = '\0';
				m_tagptr->tag = FPPTAG_INCLUDE_DIR;
				m_tagptr->data = start;
				m_tagptr++;
				start = split + 1;
			}

			m_tagptr->tag = FPPTAG_INCLUDE_DIR;
			m_tagptr->data = start;
			m_tagptr++;
		}

		void addDependency(const char* _fileName)
		{
			m_depends += " \\\n ";
			m_depends += _fileName;
		}

		bool run(const char* _input)
		{
			File file;
			file.filePath = m_filePath;
			file.fgetsPos = 0;
			m_files.push_back(file);

			m_preprocessed.clear();
			m_input = m_default;
			m_input += "\n\n";

			size_t len = strlen(_input)+1;
			char* temp = new char[len];
			bx::eolLF(temp, len, _input);
			m_input += temp;
			delete [] temp;

			fppTag* tagptr = m_tagptr;

			tagptr->tag = FPPTAG_END;
			tagptr->data = 0;
			tagptr++;

			int result = fppPreProcess(m_tags);

			for (size_t i = 0; i < m_files.size(); i++)
			{
				release(m_files[i].mem);
			}

			m_files.clear();

			return 0 == result;
		}

		bool fileExists(const char* _filePath)
		{
			if (NULL == m_callback)
			{
				FILE* file = fopen(_filePath, "rb");
				if (NULL == file)
				{
					return false;
				}
			
				fclose(file);
				return true;
			}

			return m_callback->fileExists(_filePath);
		}

		char* fgets(const char* _filePath, char* _buffer, int _size)
		{
			File *file = NULL;

			for (size_t i = 0; i < m_files.size(); i++)
			{
				if (0 == bx::stricmp(m_files[i].filePath, _filePath))
				{
					file = &m_files[i];
					break;
				}
			}

			if (!file)
			{
				File newFile;
				newFile.filePath = scratch(_filePath);
				newFile.fgetsPos = 0;

				if (NULL == m_callback)
				{
					FILE* file = fopen(_filePath, "rb");
					if (NULL != file)
					{
						newFile.mem.size = fsize(file);
						uint8_t* data = new uint8_t[newFile.mem.size];
						fread(data, 1, newFile.mem.size, file);
						newFile.mem.data = data;
						fclose(file);
					}
				}
				else
				{
					newFile.mem = m_callback->readFile(_filePath);
				}

				if (NULL == newFile.mem.data)
				{
					writeLog(m_callback, "%s: failed to read include file %s.", m_filePath, _filePath);
					return NULL;
				}

				m_files.push_back(newFile);
				file = &m_files.back();
			}

			const char *data;
			size_t size;
			if (file->filePath == m_filePath)
			{
				data = &m_input[0];
				size = m_input.size();
			}
			else
			{
				data = (const char *)file->mem.data;
				size = file->mem.size;
			}

			int ii = 0;
			for (char ch = data[file->fgetsPos]; file->fgetsPos < size && ii < _size-1; ch = data[++file->fgetsPos])
			{
				_buffer[ii++] = ch;

				if (ch == '\n' || ii == _size)
				{
					_buffer[ii] = '\0';
					file->fgetsPos++;
					return _buffer;
				}
			}

			return NULL;
		}

		static void fppDepends(char* _fileName, void* _userData)
		{
			Preprocessor* thisClass = (Preprocessor*)_userData;
			thisClass->addDependency(_fileName);
		}

		static int fppFileExists(const char* _filePath, void* _userData)
		{
			Preprocessor* thisClass = (Preprocessor*)_userData;
			return thisClass->fileExists(_filePath) ? 1 : 0;
		}

		static char* fppInput(const char* _filePath, char* _buffer, int _size, void* _userData)
		{
			Preprocessor* thisClass = (Preprocessor*)_userData;
			return thisClass->fgets(_filePath, _buffer, _size);
		}

		static void fppOutput(int _ch, void* _userData)
		{
			Preprocessor* thisClass = (Preprocessor*)_userData;
			thisClass->m_preprocessed += _ch;
		}

		static void fppError(void* _userData, char* _format, va_list _vargs)
		{
			Preprocessor* thisClass = (Preprocessor*)_userData;
			writeLog(thisClass->m_callback, _format, _vargs);
		}

		char* scratch(const char* _str)
		{
			char* result = &m_scratch[m_scratchPos];
			strcpy(result, _str);
			m_scratchPos += (uint32_t)strlen(_str)+1;
			assert(m_scratchPos < sizeof(m_scratch));
			return result;
		}

		fppTag m_tags[MAX_TAGS];
		fppTag* m_tagptr;

		std::string m_depends;
		std::string m_default;
		std::string m_input;
		std::string m_preprocessed;
		char m_scratch[16<<10];
		uint32_t m_scratchPos;
		char* m_filePath;

		struct File
		{
			char *filePath;
			uint32_t fgetsPos;
			Memory mem;
		};

		std::vector<File> m_files;
		CallbackI* m_callback;
	};

	typedef std::vector<std::string> InOut;

	uint32_t parseInOut(InOut& _inout, const char* _str, const char* _eol)
	{
		uint32_t hash = 0;
		_str = bx::strws(_str);

		if (_str < _eol)
		{
			const char* delim;
			do
			{
				delim = strpbrk(_str, " ,");
				if (NULL != delim)
				{
					delim = delim > _eol ? _eol : delim;
					std::string token;
					token.assign(_str, delim-_str);
					_inout.push_back(token);
					_str = bx::strws(delim + 1);
				}
			}
			while (delim < _eol && _str < _eol && NULL != delim);

			std::sort(_inout.begin(), _inout.end() );

			bx::HashMurmur2A murmur;
			murmur.begin();
			for (InOut::const_iterator it = _inout.begin(), itEnd = _inout.end(); it != itEnd; ++it)
			{
				murmur.add(it->c_str(), (uint32_t)it->size() );
			}
			hash = murmur.end();
		}

		return hash;
	}

	void addFragData(Preprocessor& _preprocessor, char* _data, uint32_t _idx, bool _comma)
	{
		char find[32];
		bx::snprintf(find, sizeof(find), "gl_FragData[%d]", _idx);

		char replace[32];
		bx::snprintf(replace, sizeof(replace), "gl_FragData_%d_", _idx);

		strreplace(_data, find, replace);

		_preprocessor.writef(
			" \\\n\t%sout vec4 gl_FragData_%d_ : SV_TARGET%d"
			, _comma ? ", " : "  "
			, _idx
			, _idx
			);
	}

	void voidFragData(char* _data, uint32_t _idx)
	{
		char find[32];
		bx::snprintf(find, sizeof(find), "gl_FragData[%d]", _idx);

		strreplace(_data, find, "bgfx_VoidFrag");
	}

	bool compileShader(const CompileShaderParameters& _params)
	{
		g_verbose = (_params.flags & Flags::Verbose) != 0;

		if (NULL == _params.callback)
		{
			if (NULL == _params.inputFilePath)
			{
				writeLog(_params.callback, "Input filename and callback can't both be NULL.");
				return false;
			}

			if (NULL == _params.outputFilePath)
			{
				writeLog(_params.callback, "Output filename and callback can't both be NULL.");
				return false;
			}
		}

		uint32_t glsl  = 0;
		uint32_t essl  = 0;
		uint32_t hlsl  = 2;
		uint32_t d3d   = 11;
		uint32_t metal = 0;
		if (Profile::Unspecified == _params.profile || Profile::ESSL == _params.profile)
		{
			essl = 2;
		}
		else if (Profile::HLSL3 == _params.profile)
		{
			hlsl = 3;
			d3d  = 9;
		}
		else if (Profile::HLSL4 == _params.profile || Profile::HLSL4_1 == _params.profile)
		{
			hlsl = 4;
		}
		else if (Profile::HLSL4_Level9_1 == _params.profile || Profile::HLSL4_Level9_3 == _params.profile)
		{
			hlsl = 2;
		}
		else if (Profile::HLSL5 == _params.profile)
		{
			hlsl = 5;
		}
		else if (Profile::Metal == _params.profile)
		{
			metal = 1;
		}
		else
		{
			glsl = _params.profile - Profile::GLSL_Version;
		}

		const bool depends = (_params.flags & Flags::Depends) != 0;
		const bool preprocessOnly = (_params.flags & Flags::PreprocessOnly) != 0;

		Preprocessor preprocessor(_params.inputFilePath, 0 != essl, _params.includePaths, _params.callback);

		std::string dir;

		if (NULL != _params.inputFilePath)
		{
			const char* base = bx::baseName(_params.inputFilePath);

			if (base != _params.inputFilePath)
			{
				dir.assign(_params.inputFilePath, base-_params.inputFilePath);
				preprocessor.addInclude(dir.c_str() );
			}
		}

		preprocessor.setDefaultDefine("BX_PLATFORM_ANDROID");
		preprocessor.setDefaultDefine("BX_PLATFORM_EMSCRIPTEN");
		preprocessor.setDefaultDefine("BX_PLATFORM_IOS");
		preprocessor.setDefaultDefine("BX_PLATFORM_LINUX");
		preprocessor.setDefaultDefine("BX_PLATFORM_NACL");
		preprocessor.setDefaultDefine("BX_PLATFORM_OSX");
		preprocessor.setDefaultDefine("BX_PLATFORM_WINDOWS");
		preprocessor.setDefaultDefine("BX_PLATFORM_XBOX360");
	//	preprocessor.setDefaultDefine("BGFX_SHADER_LANGUAGE_ESSL");
		preprocessor.setDefaultDefine("BGFX_SHADER_LANGUAGE_GLSL");
		preprocessor.setDefaultDefine("BGFX_SHADER_LANGUAGE_HLSL");
		preprocessor.setDefaultDefine("BGFX_SHADER_LANGUAGE_METAL");
		preprocessor.setDefaultDefine("BGFX_SHADER_TYPE_COMPUTE");
		preprocessor.setDefaultDefine("BGFX_SHADER_TYPE_FRAGMENT");
		preprocessor.setDefaultDefine("BGFX_SHADER_TYPE_VERTEX");

		char glslDefine[128];
		bx::snprintf(glslDefine, BX_COUNTOF(glslDefine), "BGFX_SHADER_LANGUAGE_GLSL=%d", essl ? 1 : glsl);

		if (Platform::Android == _params.platform)
		{
			preprocessor.setDefine("BX_PLATFORM_ANDROID=1");
			preprocessor.setDefine("BGFX_SHADER_LANGUAGE_GLSL=1");
		}
		else if (Platform::asm_js == _params.platform)
		{
			preprocessor.setDefine("BX_PLATFORM_EMSCRIPTEN=1");
			preprocessor.setDefine("BGFX_SHADER_LANGUAGE_GLSL=1");
		}
		else if (Platform::iOS == _params.platform)
		{
			preprocessor.setDefine("BX_PLATFORM_IOS=1");
			preprocessor.setDefine("BGFX_SHADER_LANGUAGE_GLSL=1");
		}
		else if (Platform::Linux == _params.platform)
		{
			preprocessor.setDefine("BX_PLATFORM_LINUX=1");
			preprocessor.setDefine(glslDefine);
		}
		else if (Platform::NaCl == _params.platform)
		{
			preprocessor.setDefine("BX_PLATFORM_NACL=1");
			preprocessor.setDefine("BGFX_SHADER_LANGUAGE_GLSL=1");
		}
		else if (Platform::OSX == _params.platform)
		{
			preprocessor.setDefine("BX_PLATFORM_OSX=1");
			preprocessor.setDefine(glslDefine);
			char temp[256];
			bx::snprintf(temp, sizeof(temp), "BGFX_SHADER_LANGUAGE_METAL=%d", metal);
			preprocessor.setDefine(temp);
		}
		else if (Platform::Windows == _params.platform)
		{
			preprocessor.setDefine("BX_PLATFORM_WINDOWS=1");
			char temp[256];
			bx::snprintf(temp, sizeof(temp), "BGFX_SHADER_LANGUAGE_HLSL=%d", hlsl);
			preprocessor.setDefine(temp);
		}
		else if (Platform::Xbox360 == _params.platform)
		{
			preprocessor.setDefine("BX_PLATFORM_XBOX360=1");
			preprocessor.setDefine("BGFX_SHADER_LANGUAGE_HLSL=3");
		}
		else
		{
			writeLog(_params.callback, "Unknown platform?!");
			return false;
		}

		preprocessor.setDefine("M_PI=3.1415926535897932384626433832795");

		switch (_params.shaderType)
		{
		case ShaderType::Compute:
			preprocessor.setDefine("BGFX_SHADER_TYPE_COMPUTE=1");
			break;

		case ShaderType::Fragment:
			preprocessor.setDefine("BGFX_SHADER_TYPE_FRAGMENT=1");
			break;

		case ShaderType::Vertex:
			preprocessor.setDefine("BGFX_SHADER_TYPE_VERTEX=1");
			break;

		default:
			writeLog(_params.callback, "Unknown shader type: %d?!", _params.shaderType);
			return false;
		}

		if (NULL != _params.preprocessorDefines)
		{
			preprocessor.setDefines(_params.preprocessorDefines);
		}

		bool compiled = false;
		bool raw = (_params.flags & Flags::Verbose) != 0;

		FILE* inputFile;
		Memory inputFileData;

		if (NULL == _params.callback)
		{
			inputFile = fopen(_params.inputFilePath, "r");
			if (NULL == inputFile)
			{
				writeLog(_params.callback, "Unable to open file '%s'.\n", _params.inputFilePath);
			}
		}
		else
		{
			inputFileData = _params.callback->readFile(_params.inputFilePath);
			if (NULL == inputFileData.data)
			{
				if (NULL == _params.inputFilePath)
				{
					writeLog(_params.callback, "Unable to read input file.\n");
				}
				else
				{
					writeLog(_params.callback, "Unable to read file '%s'.\n", _params.inputFilePath);	
				}
			}
		}

		if (NULL != inputFile || NULL != inputFileData.data)
		{
			VaryingMap varyingMap;
			std::string defaultVarying = dir + "varying.def.sc";
			const char* varyingdef = _params.varingdefPath ? _params.varingdefPath : defaultVarying.c_str();
			char* varyingData = NULL;
			if (NULL == _params.callback)
			{
				FILE* file = fopen(varyingdef, "r");
				if (NULL != file)
				{
					long size = fsize(file);
					varyingData = new char[size+1];
					size = (uint32_t)fread(varyingData, 1, size, file);
					varyingData[size] = '\0';
					fclose(file);
				}
			}
			else
			{
				Memory mem = _params.callback->readFile(varyingdef);
				if (NULL != mem.data)
				{
					varyingData = new char[mem.size+1];
					memcpy(varyingData, mem.data, mem.size);
					varyingData[mem.size] = '\0';
					release(mem);
				}
			}

			const char* parse = varyingData;
			if (NULL != parse
			&&  *parse != '\0')
			{
				preprocessor.addDependency(varyingdef);
			}

			while (NULL != parse
				&&  *parse != '\0')
			{
				parse = bx::strws(parse);
				const char* eol = strchr(parse, ';');
				if (NULL == eol)
				{
					eol = bx::streol(parse);
				}

				if (NULL != eol)
				{
					const char* precision = NULL;
					const char* interpolation = NULL;
					const char* typen = parse;

					if (0 == strncmp(typen, "lowp", 4)
					||  0 == strncmp(typen, "mediump", 7)
					||  0 == strncmp(typen, "highp", 5) )
					{
						precision = typen;
						typen = parse = bx::strws(bx::strword(parse) );
					}

					if (0 == strncmp(typen, "flat", 4)
					||  0 == strncmp(typen, "smooth", 6)
					||  0 == strncmp(typen, "noperspective", 13) )
					{
						interpolation = typen;
						typen = parse = bx::strws(bx::strword(parse) );
					}

					const char* name      = parse = bx::strws(bx::strword(parse) );
					const char* column    = parse = bx::strws(bx::strword(parse) );
					const char* semantics = parse = bx::strws((*parse == ':' ? ++parse : parse));
					const char* assign    = parse = bx::strws(bx::strword(parse) );
					const char* init      = parse = bx::strws((*parse == '=' ? ++parse : parse));

					if (typen < eol
					&&  name < eol
					&&  column < eol
					&&  ':' == *column
					&&  semantics < eol)
					{
						Varying var;
						if (NULL != precision)
						{
							var.m_precision.assign(precision, bx::strword(precision)-precision);
						}

						if (NULL != interpolation)
						{
							var.m_interpolation.assign(interpolation, bx::strword(interpolation)-interpolation);
						}

						var.m_type.assign(typen, bx::strword(typen)-typen);
						var.m_name.assign(name, bx::strword(name)-name);
						var.m_semantics.assign(semantics, bx::strword(semantics)-semantics);

						if (d3d == 9
						&&  var.m_semantics == "BITANGENT")
						{
							var.m_semantics = "BINORMAL";
						}

						if (assign < eol
						&&  '=' == *assign
						&&  init < eol)
						{
							var.m_init.assign(init, eol-init);
						}

						varyingMap.insert(std::make_pair(var.m_name, var) );
					}

					parse = bx::strws(bx::strnl(eol) );
				}
			}

			if (varyingData)
			{
				delete [] varyingData;
			}

			InOut shaderInputs;
			InOut shaderOutputs;
			uint32_t inputHash = 0;
			uint32_t outputHash = 0;

			char* data;
			char* input;
			{
				const size_t padding = 1024;
				uint32_t size;

				if (NULL == _params.callback)
				{
					uint32_t size = (uint32_t)fsize(inputFile);
					data = new char[size+padding+1];
					size = (uint32_t)fread(data, 1, size, inputFile);
				}
				else
				{
					size = inputFileData.size;
					data = new char[inputFileData.size+padding+1];
					memcpy(data, inputFileData.data, inputFileData.size);
				}

				// Compiler generates "error X3000: syntax error: unexpected end of file"
				// if input doesn't have empty line at EOF.
				data[inputFileData.size] = '\n';
				memset(&data[inputFileData.size+1], 0, padding);

				if (NULL == _params.callback)
				{
					fclose(inputFile);
				}
				else
				{
					release(inputFileData);
				}

				input = const_cast<char*>(bx::strws(data) );
				while (input[0] == '$')
				{
					const char* str = bx::strws(input+1);
					const char* eol = bx::streol(str);
					const char* nl  = bx::strnl(eol);
					input = const_cast<char*>(nl);

					if (0 == strncmp(str, "input", 5) )
					{
						str += 5;
						const char* comment = strstr(str, "//");
						eol = NULL != comment && comment < eol ? comment : eol;
						inputHash = parseInOut(shaderInputs, str, eol);
					}
					else if (0 == strncmp(str, "output", 6) )
					{
						str += 6;
						const char* comment = strstr(str, "//");
						eol = NULL != comment && comment < eol ? comment : eol;
						outputHash = parseInOut(shaderOutputs, str, eol);
					}
					else if (0 == strncmp(str, "raw", 3) )
					{
						raw = true;
						str += 3;
					}

					input = const_cast<char*>(bx::strws(input) );
				}

				if (!raw)
				{
					// To avoid commented code being recognized as used feature,
					// first preprocess pass is used to strip all comments before
					// substituting code.
					preprocessor.run(input);
					delete [] data;

					uint32_t size = (uint32_t)preprocessor.m_preprocessed.size();
					data = new char[size+padding+1];
					memcpy(data, preprocessor.m_preprocessed.c_str(), size);
					memset(&data[size], 0, padding+1);
					input = data;
				}
			}

			if (raw)
			{
				FileWriter writer(_params.callback);

				/*else if (NULL != _params.bin2cFilePath)
				{
					writer = new Bin2cWriter(_params.bin2cFilePath);
				}*/

				if (0 != writer.open(_params.outputFilePath) )
				{
					if (NULL == _params.outputFilePath)
					{
						writeLog(_params.callback, "Unable to open output file.");
					}
					else
					{
						writeLog(_params.callback, "Unable to open output file '%s'.", _params.outputFilePath);
					}

					delete[] data;
					return false;
				}

				if (ShaderType::Fragment == _params.shaderType)
				{
					writer.write(BGFX_CHUNK_MAGIC_FSH);
					writer.write(inputHash);
				}
				else if (ShaderType::Vertex == _params.shaderType)
				{
					writer.write(BGFX_CHUNK_MAGIC_VSH);
					writer.write(outputHash);
				}
				else
				{
					writer.write(BGFX_CHUNK_MAGIC_CSH);
					writer.write(outputHash);
				}

				if (0 != glsl)
				{
					writer.write(uint16_t(0) );

					uint32_t shaderSize = (uint32_t)strlen(input);
					writer.write(shaderSize);
					writer.write(input, shaderSize);
					writer.write(uint8_t(0) );

					compiled = true;
				}
				else
				{
					compiled = compileHLSLShader(_params, d3d, input, &writer);
				}

				if (0 != writer.close() )
				{
					if (NULL == _params.outputFilePath)
					{
						writeLog(_params.callback, "Unable to write output file.");
					}
					else
					{
						writeLog(_params.callback, "Unable to write output file '%s'.", _params.outputFilePath);
					}

					delete[] data;
					return false;
				}
			}
			else if (ShaderType::Compute == _params.shaderType) // Compute
			{
				char* entry = strstr(input, "void main()");
				if (NULL == entry)
				{
					writeLog(_params.callback, "Shader entry point 'void main()' is not found.\n");
				}
				else
				{
					if (0 != glsl
					||  0 != essl
					||  0 != metal)
					{
					}
					else
					{
						preprocessor.writef(
							"#define lowp\n"
							"#define mediump\n"
							"#define highp\n"
							"#define ivec2 int2\n"
							"#define ivec3 int3\n"
							"#define ivec4 int4\n"
							"#define uvec2 uint2\n"
							"#define uvec3 uint3\n"
							"#define uvec4 uint4\n"
							"#define vec2 float2\n"
							"#define vec3 float3\n"
							"#define vec4 float4\n"
							"#define mat2 float2x2\n"
							"#define mat3 float3x3\n"
							"#define mat4 float4x4\n"
							);

						entry[4] = '_';

						preprocessor.writef("#define void_main()");
						preprocessor.writef(" \\\n\tvoid main(");

						uint32_t arg = 0;

						const bool hasLocalInvocationID    = NULL != strstr(input, "gl_LocalInvocationID");
						const bool hasLocalInvocationIndex = NULL != strstr(input, "gl_LocalInvocationIndex");
						const bool hasGlobalInvocationID   = NULL != strstr(input, "gl_GlobalInvocationID");
						const bool hasWorkGroupID          = NULL != strstr(input, "gl_WorkGroupID");

						if (hasLocalInvocationID)
						{
							preprocessor.writef(
								" \\\n\t%sint3 gl_LocalInvocationID : SV_GroupThreadID"
								, arg++ > 0 ? ", " : "  "
								);
						}

						if (hasLocalInvocationIndex)
						{
							preprocessor.writef(
								" \\\n\t%sint gl_LocalInvocationIndex : SV_GroupIndex"
								, arg++ > 0 ? ", " : "  "
								);
						}

						if (hasGlobalInvocationID)
						{
							preprocessor.writef(
								" \\\n\t%sint3 gl_GlobalInvocationID : SV_DispatchThreadID"
								, arg++ > 0 ? ", " : "  "
								);
						}

						if (hasWorkGroupID)
						{
							preprocessor.writef(
								" \\\n\t%sint3 gl_WorkGroupID : SV_GroupID"
								, arg++ > 0 ? ", " : "  "
								);
						}

						preprocessor.writef(
							" \\\n\t)\n"
							);
					}

					if (preprocessor.run(input) )
					{
						if (NULL != _params.inputFilePath)
						{
							BX_TRACE("Input file: %s", _params.inputFilePath);
						}

						if (NULL != _params.outputFilePath)
						{
							BX_TRACE("Output file: %s", _params.outputFilePath);
						}

						if (preprocessOnly)
						{
							FileWriter writer(_params.callback);

							if (0 != writer.open(_params.outputFilePath) )
							{
								if (NULL == _params.outputFilePath)
								{
									writeLog(_params.callback, "Unable to open output file.");
								}
								else
								{
									writeLog(_params.callback, "Unable to open output file '%s'.", _params.outputFilePath);
								}

								delete[] data;
								return false;
							}

							writer.write(preprocessor.m_preprocessed.c_str(), (int32_t)preprocessor.m_preprocessed.size() );
						
							if (0 != writer.close())
							{
								if (NULL == _params.outputFilePath)
								{
									writeLog(_params.callback, "Unable to write output file.");
								}
								else
								{
									writeLog(_params.callback, "Unable to write output file '%s'.", _params.outputFilePath);
								}

								delete[] data;
								return false;
							}

							delete[] data;
							return true;
						}

						{
							FileWriter writer(_params.callback);

							/*else if (NULL != _params.bin2cFilePath)
							{
								writer = new Bin2cWriter(_params.bin2cFilePath);
							}*/

							if (0 != writer.open(_params.outputFilePath) )
							{
								if (NULL == _params.outputFilePath)
								{
									writeLog(_params.callback, "Unable to open output file.");
								}
								else
								{
									writeLog(_params.callback, "Unable to open output file '%s'.", _params.outputFilePath);
								}

								delete[] data;
								return false;
							}

							writer.write(BGFX_CHUNK_MAGIC_CSH);
							writer.write(outputHash);

							if (0 != glsl
							||  0 != essl)
							{
								std::string code;

								if (essl)
								{
									bx::stringPrintf(code, "#version 310 es\n");
								}
								else
								{
									bx::stringPrintf(code, "#version %d\n", glsl == 0 ? 430 : glsl);
								}

								code += preprocessor.m_preprocessed;
	#if 1
								writer.write(uint16_t(0) );

								uint32_t shaderSize = (uint32_t)code.size();
								writer.write(shaderSize);
								writer.write(code.c_str(), shaderSize);
								writer.write(uint8_t(0) );

								compiled = true;
	#else
								compiled = compileGLSLShader(shaderType, essl, code, writer);
	#endif // 0
							}
							else
							{
								compiled = compileHLSLShader(_params, d3d, preprocessor.m_preprocessed, &writer);
							}

							if (0 != writer.close())
							{
								if (NULL == _params.outputFilePath)
								{
									writeLog(_params.callback, "Unable to write output file.");
								}
								else
								{
									writeLog(_params.callback, "Unable to write output file '%s'.", _params.outputFilePath);
								}

								delete[] data;
								return false;
							}
						}

						if (compiled && depends && NULL != _params.outputFilePath)
						{
							std::string ofp = _params.outputFilePath;
							ofp += ".d";
							FileWriter writer(_params.callback);
							if (0 == writer.open(ofp.c_str() ) )
							{
								writer.writef("%s : %s\n", _params.outputFilePath, preprocessor.m_depends.c_str() );
								writer.close();
							}
						}
					}
				}
			}
			else // Vertex/Fragment
			{
				char* entry = strstr(input, "void main()");
				if (NULL == entry)
				{
					writeLog(_params.callback, "Shader entry point 'void main()' is not found.\n");
				}
				else
				{
					if (0 != glsl
					||  0 != essl
					||  0 != metal)
					{
						if (120 == glsl
						||  0   != essl)
						{
							preprocessor.writef(
								"#define ivec2 vec2\n"
								"#define ivec3 vec3\n"
								"#define ivec4 vec4\n"
								);
						}

						if (0 == essl)
						{
							// bgfx shadow2D/Proj behave like EXT_shadow_samplers
							// not as GLSL language 1.2 specs shadow2D/Proj.
							preprocessor.writef(
								"#define shadow2D(_sampler, _coord) bgfxShadow2D(_sampler, _coord).x\n"
								"#define shadow2DProj(_sampler, _coord) bgfxShadow2DProj(_sampler, _coord).x\n"
								);
						}

						for (InOut::const_iterator it = shaderInputs.begin(), itEnd = shaderInputs.end(); it != itEnd; ++it)
						{
							VaryingMap::const_iterator varyingIt = varyingMap.find(*it);
							if (varyingIt != varyingMap.end() )
							{
								const Varying& var = varyingIt->second;
								const char* name = var.m_name.c_str();

								if (0 == strncmp(name, "a_", 2)
								||  0 == strncmp(name, "i_", 2) )
								{
									preprocessor.writef("attribute %s %s %s %s;\n"
											, var.m_precision.c_str()
											, var.m_interpolation.c_str()
											, var.m_type.c_str()
											, name
											);
								}
								else
								{
									preprocessor.writef("%s varying %s %s %s;\n"
											, var.m_interpolation.c_str()
											, var.m_precision.c_str()
											, var.m_type.c_str()
											, name
											);
								}
							}
						}

						for (InOut::const_iterator it = shaderOutputs.begin(), itEnd = shaderOutputs.end(); it != itEnd; ++it)
						{
							VaryingMap::const_iterator varyingIt = varyingMap.find(*it);
							if (varyingIt != varyingMap.end() )
							{
								const Varying& var = varyingIt->second;
								preprocessor.writef("%s varying %s %s;\n"
									, var.m_interpolation.c_str()
									, var.m_type.c_str()
									, var.m_name.c_str()
									);
							}
						}
					}
					else
					{
						preprocessor.writef(
							"#define lowp\n"
							"#define mediump\n"
							"#define highp\n"
							"#define ivec2 int2\n"
							"#define ivec3 int3\n"
							"#define ivec4 int4\n"
							"#define uvec2 uint2\n"
							"#define uvec3 uint3\n"
							"#define uvec4 uint4\n"
							"#define vec2 float2\n"
							"#define vec3 float3\n"
							"#define vec4 float4\n"
							"#define mat2 float2x2\n"
							"#define mat3 float3x3\n"
							"#define mat4 float4x4\n"
							);

						if (hlsl < 4)
						{
							preprocessor.writef(
								"#define flat\n"
								"#define smooth\n"
								"#define noperspective\n"
								);
						}

						entry[4] = '_';

						if (ShaderType::Fragment == _params.shaderType)
						{
							const char* brace = strstr(entry, "{");
							if (NULL != brace)
							{
								strins(const_cast<char*>(brace+1), "\nvec4 bgfx_VoidFrag;\n");
							}

							const bool hasFragCoord   = NULL != strstr(input, "gl_FragCoord") || hlsl > 3 || hlsl == 2;
							const bool hasFragDepth   = NULL != strstr(input, "gl_FragDepth");
							const bool hasFrontFacing = NULL != strstr(input, "gl_FrontFacing");
							const bool hasPrimitiveId = NULL != strstr(input, "gl_PrimitiveID");

							bool hasFragData[8] = {};
							uint32_t numFragData = 0;
							for (uint32_t ii = 0; ii < BX_COUNTOF(hasFragData); ++ii)
							{
								char temp[32];
								bx::snprintf(temp, BX_COUNTOF(temp), "gl_FragData[%d]", ii);
								hasFragData[ii] = NULL != strstr(input, temp);
								numFragData += hasFragData[ii];
							}

							if (0 == numFragData)
							{
								// GL errors when both gl_FragColor and gl_FragData is used.
								// This will trigger the same error with HLSL compiler too.
								preprocessor.writef("#define gl_FragColor gl_FragData_0_\n");
							}

							preprocessor.writef("#define void_main()");
							preprocessor.writef(" \\\n\tvoid main(");

							uint32_t arg = 0;

							if (hasFragCoord)
							{
								preprocessor.writef(" \\\n\tvec4 gl_FragCoord : SV_POSITION");
								++arg;
							}

							for (InOut::const_iterator it = shaderInputs.begin(), itEnd = shaderInputs.end(); it != itEnd; ++it)
							{
								VaryingMap::const_iterator varyingIt = varyingMap.find(*it);
								if (varyingIt != varyingMap.end() )
								{
									const Varying& var = varyingIt->second;
									preprocessor.writef(" \\\n\t%s%s %s %s : %s"
										, arg++ > 0 ? ", " : "  "
										, interpolationDx11(var.m_interpolation.c_str() )
										, var.m_type.c_str()
										, var.m_name.c_str()
										, var.m_semantics.c_str()
										);
								}
							}

							addFragData(preprocessor, input, 0, arg++ > 0);

							const uint32_t maxRT = d3d > 9 ? BX_COUNTOF(hasFragData) : 4;

							for (uint32_t ii = 1; ii < BX_COUNTOF(hasFragData); ++ii)
							{
								if (ii < maxRT)
								{
									if (hasFragData[ii])
									{
										addFragData(preprocessor, input, ii, arg++ > 0);
									}
								}
								else
								{
									voidFragData(input, ii);
								}
							}

							if (hasFragDepth)
							{
								preprocessor.writef(
									" \\\n\t%sout float gl_FragDepth : SV_DEPTH"
									, arg++ > 0 ? ", " : "  "
									);
							}

							if (hasFrontFacing
							&&  hlsl >= 3)
							{
								preprocessor.writef(
									" \\\n\t%sfloat __vface : VFACE"
									, arg++ > 0 ? ", " : "  "
									);
							}

							if (hasPrimitiveId)
							{
								if (d3d > 9)
								{
									preprocessor.writef(
										" \\\n\t%suint gl_PrimitiveID : SV_PrimitiveID"
										, arg++ > 0 ? ", " : "  "
										);
								}
								else
								{
									writeLog(_params.callback, "PrimitiveID builtin is not supported by this D3D9 HLSL.\n");
									delete[] data;
									return false;
								}
							}

							preprocessor.writef(
								" \\\n\t)\n"
								);

							if (hasFrontFacing)
							{
								if (hlsl >= 3)
								{
									preprocessor.writef(
										"#define gl_FrontFacing (__vface <= 0.0)\n"
										);
								}
								else
								{
									preprocessor.writef(
										"#define gl_FrontFacing false\n"
										);
								}
							}
						}
						else if (ShaderType::Vertex == _params.shaderType)
						{
							const char* brace = strstr(entry, "{");
							if (NULL != brace)
							{
								const char* end = bx::strmb(brace, '{', '}');
								if (NULL != end)
								{
									strins(const_cast<char*>(end), "__RETURN__;\n");
								}
							}

							preprocessor.writef(
								"struct Output\n"
								"{\n"
								"\tvec4 gl_Position : SV_POSITION;\n"
								"#define gl_Position _varying_.gl_Position\n"
								);
							for (InOut::const_iterator it = shaderOutputs.begin(), itEnd = shaderOutputs.end(); it != itEnd; ++it)
							{
								VaryingMap::const_iterator varyingIt = varyingMap.find(*it);
								if (varyingIt != varyingMap.end() )
								{
									const Varying& var = varyingIt->second;
									preprocessor.writef("\t%s %s : %s;\n", var.m_type.c_str(), var.m_name.c_str(), var.m_semantics.c_str() );
									preprocessor.writef("#define %s _varying_.%s\n", var.m_name.c_str(), var.m_name.c_str() );
								}
							}
							preprocessor.writef(
								"};\n"
								);

							preprocessor.writef("#define void_main() \\\n");
							preprocessor.writef("Output main(");
							bool first = true;
							for (InOut::const_iterator it = shaderInputs.begin(), itEnd = shaderInputs.end(); it != itEnd; ++it)
							{
								VaryingMap::const_iterator varyingIt = varyingMap.find(*it);
								if (varyingIt != varyingMap.end() )
								{
									const Varying& var = varyingIt->second;
									preprocessor.writef("%s%s %s : %s\\\n", first ? "" : "\t, ", var.m_type.c_str(), var.m_name.c_str(), var.m_semantics.c_str() );
									first = false;
								}
							}
							preprocessor.writef(
								") \\\n"
								"{ \\\n"
								"\tOutput _varying_;"
								);

							for (InOut::const_iterator it = shaderOutputs.begin(), itEnd = shaderOutputs.end(); it != itEnd; ++it)
							{
								VaryingMap::const_iterator varyingIt = varyingMap.find(*it);
								if (varyingIt != varyingMap.end() )
								{
									const Varying& var = varyingIt->second;
									preprocessor.writef(" \\\n\t%s = %s;", var.m_name.c_str(), var.m_init.c_str() );
								}
							}

							preprocessor.writef(
								"\n#define __RETURN__ \\\n"
								"\t} \\\n"
								"\treturn _varying_"
								);
						}
					}

					if (preprocessor.run(input) )
					{
						if (NULL != _params.inputFilePath)
						{
							BX_TRACE("Input file: %s", _params.inputFilePath);
						}

						if (NULL != _params.outputFilePath)
						{
							BX_TRACE("Output file: %s", _params.outputFilePath);
						}

						if (preprocessOnly)
						{
							FileWriter writer(_params.callback);

							if (0 != writer.open(_params.outputFilePath) )
							{
								if (NULL == _params.outputFilePath)
								{
									writeLog(_params.callback, "Unable to open output file.");
								}
								else
								{
									writeLog(_params.callback, "Unable to open output file '%s'.", _params.outputFilePath);
								}

								delete[] data;
								return false;
							}

							if (0 != glsl)
							{
								if (Profile::Unspecified == _params.profile)
								{
									writer.writef(
											"#ifdef GL_ES\n"
											"precision highp float;\n"
											"#endif // GL_ES\n\n"
										);
								}
							}
							writer.write(preprocessor.m_preprocessed.c_str(), (int32_t)preprocessor.m_preprocessed.size() );

							if (0 != writer.close())
							{
								if (NULL == _params.outputFilePath)
								{
									writeLog(_params.callback, "Unable to write output file.");
								}
								else
								{
									writeLog(_params.callback, "Unable to write output file '%s'.", _params.outputFilePath);
								}

								delete[] data;
								return false;
							}

							delete[] data;
							return true;
						}

						{
							FileWriter writer(_params.callback);

							/*else if (NULL != _params.bin2cFilePath)
							{
								writer = new Bin2cWriter(_params.bin2cFilePath);
							}*/

							if (0 != writer.open(_params.outputFilePath) )
							{
								if (NULL == _params.outputFilePath)
								{
									writeLog(_params.callback, "Unable to open output file.");
								}
								else
								{
									writeLog(_params.callback, "Unable to open output file '%s'.", _params.outputFilePath);
								}

								delete[] data;
								return false;
							}

							if (ShaderType::Fragment == _params.shaderType)
							{
								writer.write(BGFX_CHUNK_MAGIC_FSH);
								writer.write(inputHash);
							}
							else if (ShaderType::Vertex == _params.shaderType)
							{
								writer.write(BGFX_CHUNK_MAGIC_VSH);
								writer.write(outputHash);
							}
							else
							{
								writer.write(BGFX_CHUNK_MAGIC_CSH);
								writer.write(outputHash);
							}

							if (0 != glsl
							||  0 != essl)
							{
								std::string code;

								bool hasTextureLod = NULL != bx::findIdentifierMatch(input, s_ARB_shader_texture_lod /*EXT_shader_texture_lod*/);

								if (0 == essl)
								{
									bx::stringPrintf(code, "#version %d\n", _params.profile >= Profile::GLSL_Version ? _params.profile - Profile::GLSL_Version : 0);

									bx::stringPrintf(code
										, "#define bgfxShadow2D shadow2D\n"
											"#define bgfxShadow2DProj shadow2DProj\n"
										);

									if (hasTextureLod
									&&  130 > glsl)
									{
										bx::stringPrintf(code
											, "#extension GL_ARB_shader_texture_lod : enable\n"
											);
									}
								}
								else
								{
									// Pretend that all extensions are available.
									// This will be stripped later.
									if (hasTextureLod)
									{
										bx::stringPrintf(code
											, "#extension GL_EXT_shader_texture_lod : enable\n"
												"#define texture2DLod texture2DLodEXT\n"
												"#define texture2DProjLod texture2DProjLodEXT\n"
												"#define textureCubeLod textureCubeLodEXT\n"
	//										  "#define texture2DGrad texture2DGradEXT\n"
	//										  "#define texture2DProjGrad texture2DProjGradEXT\n"
	//										  "#define textureCubeGrad textureCubeGradEXT\n"
											);
									}

									if (NULL != bx::findIdentifierMatch(input, s_OES_standard_derivatives) )
									{
										bx::stringPrintf(code, "#extension GL_OES_standard_derivatives : enable\n");
									}

									if (NULL != bx::findIdentifierMatch(input, s_OES_texture_3D) )
									{
										bx::stringPrintf(code, "#extension GL_OES_texture_3D : enable\n");
									}

									if (NULL != bx::findIdentifierMatch(input, s_EXT_shadow_samplers) )
									{
										bx::stringPrintf(code
											, "#extension GL_EXT_shadow_samplers : enable\n"
												"#define shadow2D shadow2DEXT\n"
												"#define shadow2DProj shadow2DProjEXT\n"
											);
									}

									if (NULL != bx::findIdentifierMatch(input, "gl_FragDepth") )
									{
										bx::stringPrintf(code
											, "#extension GL_EXT_frag_depth : enable\n"
												"#define gl_FragDepth gl_FragDepthEXT\n"
											);
									}
								}

								code += preprocessor.m_preprocessed;
								compiled = compileGLSLShader(_params
										, essl
										, code
										, &writer
										);
							}
							else if (0 != metal)
							{
								compiled = compileGLSLShader(_params
										, BX_MAKEFOURCC('M', 'T', 'L', 0)
										, preprocessor.m_preprocessed
										, &writer
										);
							}
							else
							{
								compiled = compileHLSLShader(_params
										, d3d
										, preprocessor.m_preprocessed
										, &writer
										);
							}

							if (0 != writer.close())
							{
								if (NULL == _params.outputFilePath)
								{
									writeLog(_params.callback, "Unable to write output file.");
								}
								else
								{
									writeLog(_params.callback, "Unable to write output file '%s'.", _params.outputFilePath);
								}

								delete[] data;
								return false;
							}
						}

						if (compiled && depends && NULL != _params.outputFilePath)
						{
							std::string ofp = _params.outputFilePath;
							ofp += ".d";
							FileWriter writer(_params.callback);
							if (0 == writer.open(ofp.c_str() ) )
							{
								writer.writef("%s : %s\n", _params.outputFilePath, preprocessor.m_depends.c_str() );
								writer.close();
							}
						}
					}
				}
			}

			delete [] data;
		}

		if (compiled)
		{
			return true;
		}

		if (NULL != _params.outputFilePath)
		{
			remove(_params.outputFilePath);
		}

		writeLog(_params.callback, "Failed to build shader.\n");
		return false;
	}
} // namespace shaderc

#ifndef SHADERC_LIB
namespace shaderc
{
	struct CliCallback : public CallbackI
	{
		~CliCallback() {}

		virtual bool fileExists(const char* _filename) BX_OVERRIDE
		{
			FILE* file = fopen(_filename, "rb");
			if (NULL == file)
			{
				return false;
			}
			
			fclose(file);
			return true;
		}

		virtual Memory readFile(const char* _filename) BX_OVERRIDE
		{
			Memory mem;
			FILE* file = fopen(_filename, "rb");
			if (NULL != file)
			{
				mem.size = (uint32_t)fsize(file);
				uint8_t* data = new uint8_t[mem.size];
				fread(data, 1, mem.size, file);
				mem.data = data;
				fclose(file);
			}
			return mem;
		}

		virtual bool writeFile(const char* _filename, const void* _data, int32_t _size) BX_OVERRIDE
		{
			bx::CrtFileWriter out;
			if (0 == out.open(_filename) )
			{
				out.write(_data, _size);
				if (0 == out.close() )
				{
					return true;
				}
			}
			
			return false;
		}

		virtual void writeLog(const char* _str) BX_OVERRIDE
		{
			fprintf(stderr, _str);
		}
	};

	// c - compute
	// d - domain
	// f - fragment
	// g - geometry
	// h - hull
	// v - vertex
	//
	// OpenGL #version Features Direct3D Features Shader Model
	// 2.1    120      vf       9.0      vf       2.0
	// 3.0    130
	// 3.1    140
	// 3.2    150      vgf
	// 3.3    330               10.0     vgf      4.0
	// 4.0    400      vhdgf
	// 4.1    410
	// 4.2    420               11.0     vhdgf+c  5.0
	// 4.3    430      vhdgf+c
	// 4.4    440

	void help(const char* _error = NULL)
	{
		if (NULL != _error)
		{
			fprintf(stderr, "Error:\n%s\n\n", _error);
		}

		fprintf(stderr
			, "shaderc, bgfx shader compiler tool\n"
			  "Copyright 2011-2015 Branimir Karadzic. All rights reserved.\n"
			  "License: http://www.opensource.org/licenses/BSD-2-Clause\n\n"
			);

		fprintf(stderr
			, "Usage: shaderc -f <in> -o <out> --type <v/f> --platform <platform>\n"

			  "\n"
			  "Options:\n"
			  "  -f <file path>                Input file path.\n"
			  "  -i <include path>             Include path (for multiple paths use semicolon).\n"
			  "  -o <file path>                Output file path.\n"
			  "      --bin2c <file path>       Generate C header file.\n"
			  "      --depends                 Generate makefile style depends file.\n"
			  "      --platform <platform>     Target platform.\n"
			  "           android\n"
			  "           asm.js\n"
			  "           ios\n"
			  "           linux\n"
			  "           nacl\n"
			  "           osx\n"
			  "           windows\n"
			  "      --preprocess              Preprocess only.\n"
			  "      --raw                     Do not process shader. No preprocessor, and no glsl-optimizer (GLSL only).\n"
			  "      --type <type>             Shader type (vertex, fragment)\n"
			  "      --varyingdef <file path>  Path to varying.def.sc file.\n"
			  "      --verbose                 Verbose.\n"

			  "\n"
			  "Options (DX9 and DX11 only):\n"

			  "\n"
			  "      --debug                   Debug information.\n"
			  "      --disasm                  Disassemble compiled shader.\n"
			  "  -p, --profile <profile>       Shader model (f.e. ps_3_0).\n"
			  "  -O <level>                    Optimization level (0, 1, 2, 3).\n"
			  "      --Werror                  Treat warnings as errors.\n"

			  "\n"
			  "For additional information, see https://github.com/bkaradzic/bgfx\n"
			);
	}

	static int main(bx::CommandLine _cmdLine)
	{
		if (_cmdLine.hasArg('h', "help") )
		{
			help();
			return EXIT_FAILURE;
		}
		
		ShaderType::Enum shaderType;
		const char* shaderTypeStr = _cmdLine.findOption('\0', "type");
		if (NULL == shaderTypeStr)
		{
			help("Must specify shader type.");
			return EXIT_FAILURE;
		}

		switch (tolower(shaderTypeStr[0]) )
		{
		case 'c':
			shaderType = ShaderType::Compute;
			break;

		case 'f':
			shaderType = ShaderType::Fragment;
			break;

		case 'v':
			shaderType = ShaderType::Vertex;
			break;

		default:
			fprintf(stderr, "Unknown type: %s?!", shaderTypeStr);
			return EXIT_FAILURE;
		}

		Platform::Enum platform;
		const char* platformStr = _cmdLine.findOption('\0', "platform");
		if (NULL == platformStr)
		{
			help("Must specify platform.");
			return EXIT_FAILURE;
		}

		if (0 == bx::stricmp(platformStr, "android") )
		{
			platform = Platform::Android;
		}
		else if (0 == bx::stricmp(platformStr, "asm.js") )
		{
			platform = Platform::asm_js;
		}
		else if (0 == bx::stricmp(platformStr, "ios") )
		{
			platform = Platform::iOS;
		}
		else if (0 == bx::stricmp(platformStr, "linux") )
		{
			platform = Platform::Linux;
		}
		else if (0 == bx::stricmp(platformStr, "nacl") )
		{
			platform = Platform::NaCl;
		}
		else if (0 == bx::stricmp(platformStr, "osx") )
		{
			platform = Platform::OSX;
		}
		else if (0 == bx::stricmp(platformStr, "windows") )
		{
			platform = Platform::Windows;
		}
		else if (0 == bx::stricmp(platformStr, "xbox360") )
		{
			platform = Platform::Xbox360;
		}
		else
		{
			fprintf(stderr, "Unknown platform %s?!", platformStr);
			return EXIT_FAILURE;
		}

		CompileShaderParameters params(shaderType, platform);
		CliCallback callback;
		params.callback = &callback;
		params.flags = 0;
		params.flags |= _cmdLine.hasArg("verbose") ? Flags::Verbose : 0;
		params.flags |= _cmdLine.hasArg("raw") ? Flags::Raw : 0;
		params.flags |= _cmdLine.hasArg("depends") ? Flags::Depends : 0;
		params.flags |= _cmdLine.hasArg("preprocess") ? Flags::PreprocessOnly : 0;
		params.flags |= _cmdLine.hasArg("debug") ? Flags::D3DDebug : 0;
		params.flags |= _cmdLine.hasArg("Werror") ? Flags::D3DWarningsAsErrors : 0;
		params.flags |= _cmdLine.hasArg("avoid-flow-control") ? Flags::D3DAvoidFlowControl : 0;
		params.flags |= _cmdLine.hasArg("no-preshader") ? Flags::D3DNoPreshader : 0;
		params.flags |= _cmdLine.hasArg("partial-precision") ? Flags::D3DPartialPrecision : 0;
		params.flags |= _cmdLine.hasArg("prefer-flow-control") ? Flags::D3DPreferFlowControl : 0;
		params.flags |= _cmdLine.hasArg("backwards-compatibility") ? Flags::D3DBackwardsCompatibility : 0;
		params.flags |= _cmdLine.hasArg("disasm") ? Flags::D3DDisassemble : 0;

		if (_cmdLine.hasArg(params.d3dOptimizationLevel, 'O') )
		{
			params.flags |= Flags::D3DOptimize;
		}

		params.inputFilePath = _cmdLine.findOption('f');
		if (NULL == params.inputFilePath)
		{
			help("Shader file name must be specified.");
			return EXIT_FAILURE;
		}

		params.includePaths = _cmdLine.findOption('i');

		params.outputFilePath = _cmdLine.findOption('o');
		if (NULL == params.outputFilePath)
		{
			help("Output file name must be specified.");
			return EXIT_FAILURE;
		}

		if (_cmdLine.hasArg("bin2c") )
		{
			params.bin2cFilePath = _cmdLine.findOption("bin2c");
			if (NULL == params.bin2cFilePath)
			{
				params.bin2cFilePath = bx::baseName(params.outputFilePath);
				uint32_t len = (uint32_t)strlen(params.bin2cFilePath);
				char* temp = (char*)alloca(len+1);
				for (char *out = temp; *params.bin2cFilePath != '\0';)
				{
					char ch = *params.bin2cFilePath++;
					if (isalnum(ch) )
					{
						*out++ = ch;
					}
					else
					{
						*out++ = '_';
					}
				}
				temp[len] = '\0';

				params.bin2cFilePath = temp;
			}
		}

		params.varingdefPath = _cmdLine.findOption("varyingdef");

		const char* profile = _cmdLine.findOption('p', "profile");
		if (NULL != profile)
		{
			if (0 == strncmp(&profile[1], "s_3", 3) )
			{
				params.profile = Profile::HLSL3;
			}
			else if (0 == strncmp(&profile[1], "s_4_1", 5) )
			{
				params.profile = Profile::HLSL4_1;
			}
			else if (0 == strncmp(&profile[1], "s_4", 3) )
			{
				params.profile = Profile::HLSL4;
			}
			else if (0 == strncmp(&profile[1], "s_4_0_level_9_1", 15) )
			{
				params.profile = Profile::HLSL4_Level9_1;
			}
			else if (0 == strncmp(&profile[1], "s_4_0_level_9_3", 15) )
			{
				params.profile = Profile::HLSL4_Level9_3;
			}
			else if (0 == strncmp(&profile[1], "s_5", 3) )
			{
				params.profile = Profile::HLSL5;
			}
			else if (0 == strcmp(profile, "metal") )
			{
				params.profile = Profile::Metal;
			}
			else
			{
				params.profile = Profile::Enum(Profile::GLSL_Version + atoi(profile) );
			}
		}

		return compileShader(params) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
} // namespace shaderc

int main(int _argc, const char* _argv[])
{
	bx::CommandLine cmdLine(_argc, _argv);
	return shaderc::main(cmdLine);	
}
#endif
