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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Main.h"

namespace renderer {

bgfx::VertexDecl Vertex::decl;

uint8_t g_gammaTable[g_gammaTableSize];
bool g_hardwareGammaEnabled;
ConsoleVariables g_cvars;
const uint8_t *g_externalVisData = nullptr;
MaterialCache *g_materialCache = nullptr;
ModelCache *g_modelCache = nullptr;
TextureCache *g_textureCache = nullptr;

float g_sinTable[g_funcTableSize];
float g_squareTable[g_funcTableSize];
float g_triangleTable[g_funcTableSize];
float g_sawToothTable[g_funcTableSize];
float g_inverseSawToothTable[g_funcTableSize];

bool DrawCall::operator<(const DrawCall &other) const
{
	assert(material);
	assert(other.material);

	if (material->sort < other.material->sort)
	{
		return true;
	}
	else if (material->sort == other.material->sort)
	{
		if (sort < other.sort)
		{
			return true;
		}
		else if (sort == other.sort)
		{
			if (material->index < other.material->index)
				return true;
		}
	}

	return false;
}

void WarnOnce(WarnOnceId::Enum id)
{
	static bool warned[WarnOnceId::Num];

	if (!warned[id])
	{
		interface::PrintWarningf("BGFX transient buffer alloc failed\n");
		warned[id] = true;
	}
}

namespace main {

std::unique_ptr<Main> s_main;

const FrameBuffer Main::defaultFb;

const mat4 Main::toOpenGlMatrix
(
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
);

void BgfxCallback::fatal(bgfx::Fatal::Enum _code, const char* _str)
{
	if (bgfx::Fatal::DebugCheck == _code)
	{
		bx::debugBreak();
	}
	else
	{
		BX_TRACE("0x%08x: %s", _code, _str);
		BX_UNUSED(_code, _str);
		abort();
	}
}

void BgfxCallback::traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList)
{
	char temp[2048];
	char* out = temp;
	int32_t len   = bx::snprintf(out, sizeof(temp), "%s (%d): ", _filePath, _line);
	int32_t total = len + bx::vsnprintf(out + len, sizeof(temp)-len, _format, _argList);
	if ( (int32_t)sizeof(temp) < total)
	{
		out = (char*)alloca(total+1);
		memcpy(out, temp, len);
		bx::vsnprintf(out + len, total-len, _format, _argList);
	}
	out[total] = '\0';
	bx::debugOutput(out);
}

struct ImageWriteBuffer
{
	std::vector<uint8_t> *data;
	size_t bytesWritten;
};

static void ImageWriteCallback(void *context, void *data, int size)
{
	auto buffer = (ImageWriteBuffer *)context;

	if (buffer->data->size() < buffer->bytesWritten + size)
	{
		buffer->data->resize(buffer->bytesWritten + size);
	}

	memcpy(&buffer->data->data()[buffer->bytesWritten], data, size);
	buffer->bytesWritten += size;
}

static void ImageWriteCallbackConst(void *context, const void *data, int size)
{
	ImageWriteCallback(context, (void *)data, size);
}

void BgfxCallback::screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t _size, bool _yflip)
{
	const int nComponents = 4;
	const bool silent = _filePath[0] == 'y';
	_filePath++;
	const char *extension = util::GetExtension(_filePath);
	const bool writeAsPng = !util::Stricmp(extension, "png");
	const uint32_t outputPitch = writeAsPng ? _pitch : _width * nComponents; // PNG can use any pitch, others can't.

	// Convert from BGRA to RGBA, and flip y if needed.
	const size_t requiredSize = outputPitch * _height;

	if (screenShotDataBuffer_.size() < requiredSize)
	{
		screenShotDataBuffer_.resize(requiredSize);
	}

	for (uint32_t y = 0; y < _height; y++)
	{
		for (uint32_t x = 0; x < _width; x++)
		{
			auto colorIn = &((const uint8_t *)_data)[x * nComponents + (_yflip ? _height - 1 - y : y) * _pitch];
			uint8_t *colorOut = &screenShotDataBuffer_[x * nComponents + y * outputPitch];
			colorOut[0] = colorIn[2];
			colorOut[1] = colorIn[1];
			colorOut[2] = colorIn[0];
			colorOut[3] = 255;

			// Apply gamma correction.
			if (g_hardwareGammaEnabled)
			{
				colorOut[0] = g_gammaTable[colorOut[0]];
				colorOut[1] = g_gammaTable[colorOut[1]];
				colorOut[2] = g_gammaTable[colorOut[2]];
			}
		}
	}

	// Write to file buffer.
	ImageWriteBuffer buffer;
	buffer.data = &screenShotFileBuffer_;
	buffer.bytesWritten = 0;

	if (writeAsPng)
	{
		if (!stbi_write_png_to_func(ImageWriteCallback, &buffer, _width, _height, nComponents, screenShotDataBuffer_.data(), (int)outputPitch))
		{
			interface::Printf("Screenshot: error writing png file\n");
			return;
		}
	}
	else if (!util::Stricmp(extension, "jpg"))
	{
		if (!stbi_write_jpg_to_func(ImageWriteCallback, &buffer, _width, _height, nComponents, screenShotDataBuffer_.data(), g_cvars.screenshotJpegQuality.getInt()))
		{
			interface::Printf("Screenshot: error writing jpg file\n");
			return;
		}
	}
	else
	{
		if (!stbi_write_tga_to_func(ImageWriteCallback, &buffer, _width, _height, nComponents, screenShotDataBuffer_.data()))
		{
			interface::Printf("Screenshot: error writing tga file\n");
			return;
		}
	}

	// Write file buffer to file.
	if (buffer.bytesWritten > 0)
	{
		interface::FS_WriteFile(_filePath, buffer.data->data(), buffer.bytesWritten);
	}

	if (!silent)
		interface::Printf("Wrote %s\n", _filePath);
}

void AddDynamicLightToScene(const DynamicLight &light)
{
	s_main->dlightManager->add(s_main->frameNo, light);
}

void AddEntityToScene(const Entity &entity)
{
	s_main->sceneEntities.push_back(entity);
}

void AddPolyToScene(qhandle_t hShader, int nVerts, const polyVert_t *verts, int nPolys)
{
	const size_t firstVertex = s_main->scenePolygonVertices.size();
	s_main->scenePolygonVertices.insert(s_main->scenePolygonVertices.end(), verts, &verts[nPolys * nVerts]);

	for (int i = 0; i < nPolys; i++)
	{
		Main::Polygon p;
		p.material = s_main->materialCache->getMaterial(hShader); 
		p.firstVertex = uint32_t(firstVertex + i * nVerts);
		p.nVertices = nVerts;
		Bounds bounds;
		bounds.setupForAddingPoints();

		for (size_t j = 0; j < p.nVertices; j++)
		{
			bounds.addPoint(s_main->scenePolygonVertices[p.firstVertex + j].xyz);
		}

		p.fogIndex = world::FindFogIndex(bounds);
		s_main->scenePolygons.push_back(p);
	}
}

bool AreWaterReflectionsEnabled()
{
	return s_main->waterReflectionsEnabled;
}

bool AreExtraDynamicLightsEnabled()
{
	return s_main->extraDynamicLightsEnabled;
}

#define NOISE_PERM(a) s_main->noisePerm[(a) & (s_main->noiseSize - 1)]
#define NOISE_TABLE(x, y, z, t) s_main->noiseTable[NOISE_PERM(x + NOISE_PERM(y + NOISE_PERM(z + NOISE_PERM(t))))]
#define NOISE_LERP( a, b, w ) ( ( a ) * ( 1.0f - ( w ) ) + ( b ) * ( w ) )

float CalculateNoise(float x, float y, float z, float t)
{
	int i;
	int ix, iy, iz, it;
	float fx, fy, fz, ft;
	float front[4];
	float back[4];
	float fvalue, bvalue, value[2], finalvalue;

	ix = (int)floor(x);
	fx = x - ix;
	iy = (int)floor(y);
	fy = y - iy;
	iz = (int)floor(z);
	fz = z - iz;
	it = (int)floor(t);
	ft = t - it;

	for (i = 0; i < 2; i++)
	{
		front[0] = NOISE_TABLE(ix, iy, iz, it + i);
		front[1] = NOISE_TABLE(ix + 1, iy, iz, it + i);
		front[2] = NOISE_TABLE(ix, iy + 1, iz, it + i);
		front[3] = NOISE_TABLE(ix + 1, iy + 1, iz, it + i);

		back[0] = NOISE_TABLE(ix, iy, iz + 1, it + i);
		back[1] = NOISE_TABLE(ix + 1, iy, iz + 1, it + i);
		back[2] = NOISE_TABLE(ix, iy + 1, iz + 1, it + i);
		back[3] = NOISE_TABLE(ix + 1, iy + 1, iz + 1, it + i);

		fvalue = NOISE_LERP(NOISE_LERP(front[0], front[1], fx), NOISE_LERP(front[2], front[3], fx), fy);
		bvalue = NOISE_LERP(NOISE_LERP(back[0], back[1], fx), NOISE_LERP(back[2], back[3], fx), fy);

		value[i] = NOISE_LERP(fvalue, bvalue, fz);
	}

	finalvalue = NOISE_LERP(value[0], value[1], ft);

	return finalvalue;
}

DebugDraw DebugDrawFromString(const char *s)
{
	if (util::Stricmp(s, "bloom") == 0)
		return DebugDraw::Bloom;
	else if (util::Stricmp(s, "depth") == 0)
		return DebugDraw::Depth;
	else if (util::Stricmp(s, "dlight") == 0)
		return DebugDraw::DynamicLight;
	else if (util::Stricmp(s, "lightmap") == 0)
		return DebugDraw::Lightmap;
	else if (util::Stricmp(s, "reflection") == 0)
		return DebugDraw::Reflection;
	else if (util::Stricmp(s, "shadow") == 0)
		return DebugDraw::Shadow;
	else if (util::Stricmp(s, "smaa") == 0)
		return DebugDraw::SMAA;

	return DebugDraw::None;
}

void DebugPrint(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	char text[1024];
	util::Vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	const uint16_t fontHeight = 16;
	const uint16_t maxY = window::GetHeight() / fontHeight;
	const uint16_t columnWidth = 32;
	uint16_t x = s_main->debugTextY / maxY * columnWidth;
	uint16_t y = s_main->debugTextY % maxY;
	bgfx::dbgTextPrintf(x, y, 0x4f, text);
	s_main->debugTextThisFrame = true;
	s_main->debugTextY++;
}

const Entity *GetCurrentEntity()
{
	return s_main->currentEntity;
}

float GetFloatTime()
{
	return s_main->floatTime;
}

Transform GetMainCameraTransform()
{
	return s_main->mainCameraTransform;
}

const SunLight &GetSunLight()
{
	return s_main->sunLight;
}

bool IsCameraMirrored()
{
	return s_main->isCameraMirrored;
}

bool IsLerpTextureAnimationEnabled()
{
	return s_main->lerpTextureAnimationEnabled;
}

bool IsMaxAnisotropyEnabled()
{
	return s_main->maxAnisotropyEnabled;
}

bool IsMsaa(AntiAliasing aa)
{
	return aa >= AntiAliasing::MSAA2x && aa <= AntiAliasing::MSAA16x;
}

static int Font_ReadInt(const uint8_t *data, int *offset)
{
	assert(data && offset);
	int i = data[*offset] + (data[*offset + 1] << 8) + (data[*offset + 2] << 16) + (data[*offset + 3] << 24);
	*offset += 4;
	return i;
}

static float Font_ReadFloat(const uint8_t *data, int *offset)
{
	assert(data && offset);
	uint8_t temp[4];
#if defined Q3_BIG_ENDIAN
	temp[0] = data[*offset + 3];
	temp[1] = data[*offset + 2];
	temp[2] = data[*offset + 1];
	temp[3] = data[*offset + 0];
#else
	temp[0] = data[*offset + 0];
	temp[1] = data[*offset + 1];
	temp[2] = data[*offset + 2];
	temp[3] = data[*offset + 3];
#endif
	*offset += 4;
	return *((float *)temp);
}

void RegisterFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	if (!fontName)
	{
		interface::Printf("RE_RegisterFont: called with empty name\n");
		return;
	}

	if (pointSize <= 0)
		pointSize = 12;

	if (s_main->nFonts >= s_main->maxFonts)
	{
		interface::PrintWarningf("RE_RegisterFont: Too many fonts registered already.\n");
		return;
	}

	char name[1024];
	util::Sprintf(name, sizeof(name), "fonts/fontImage_%i.dat", pointSize);

	for (int i = 0; i < s_main->nFonts; i++)
	{
		if (util::Stricmp(name, s_main->fonts[i].name) == 0)
		{
			memcpy(font, &s_main->fonts[i], sizeof(fontInfo_t));
			return;
		}
	}

	long len = interface::FS_ReadFile(name, NULL);

	if (len != sizeof(fontInfo_t))
		return;

	int offset = 0;
	uint8_t *data;
	interface::FS_ReadFile(name, &data);

	for (int i = 0; i < GLYPHS_PER_FONT; i++)
	{
		font->glyphs[i].height = Font_ReadInt(data, &offset);
		font->glyphs[i].top = Font_ReadInt(data, &offset);
		font->glyphs[i].bottom = Font_ReadInt(data, &offset);
		font->glyphs[i].pitch = Font_ReadInt(data, &offset);
		font->glyphs[i].xSkip = Font_ReadInt(data, &offset);
		font->glyphs[i].imageWidth = Font_ReadInt(data, &offset);
		font->glyphs[i].imageHeight = Font_ReadInt(data, &offset);
		font->glyphs[i].s = Font_ReadFloat(data, &offset);
		font->glyphs[i].t = Font_ReadFloat(data, &offset);
		font->glyphs[i].s2 = Font_ReadFloat(data, &offset);
		font->glyphs[i].t2 = Font_ReadFloat(data, &offset);
		font->glyphs[i].glyph = Font_ReadInt(data, &offset);
		util::Strncpyz(font->glyphs[i].shaderName, (const char *)&data[offset], sizeof(font->glyphs[i].shaderName));
		offset += sizeof(font->glyphs[i].shaderName);
	}

	font->glyphScale = Font_ReadFloat(data, &offset);
	util::Strncpyz(font->name, name, sizeof(font->name));

	for (int i = GLYPH_START; i <= GLYPH_END; i++)
	{
		Material *m = s_main->materialCache->findMaterial(font->glyphs[i].shaderName, MaterialLightmapId::StretchPic, false);
		font->glyphs[i].glyph = m->defaultShader ? 0 : m->index;
	}

	memcpy(&s_main->fonts[s_main->nFonts++], font, sizeof(fontInfo_t));
	interface::FS_FreeReadFile(data);
}

bool SampleLight(vec3 position, vec3 *ambientLight, vec3 *directedLight, vec3 *lightDir)
{
	if (!world::HasLightGrid())
		return false;

	world::SampleLightGrid(position, ambientLight, directedLight, lightDir);
	return true;
}

void SetColor(vec4 c)
{
	s_main->stretchPicColor = c;
}

void SetSunLight(const SunLight &sunLight)
{
	s_main->sunLight = sunLight;
}

void SetWindowGamma()
{
	if (!g_hardwareGammaEnabled)
		return;
		
	const float gamma = math::Clamped(g_cvars.gamma.getFloat(), 0.5f, 3.0f);

	for (size_t i = 0; i < g_gammaTableSize; i++)
	{
		int value = int(i);

		if (gamma != 1.0f)
		{
			value = int(255 * pow(i / 255.0f, 1.0f / gamma) + 0.5f);
		}

		g_gammaTable[i] = math::Clamped(value * (int)g_overbrightFactor, 0, 255);
	}

	window::SetGamma(g_gammaTable, g_gammaTable, g_gammaTable);
}

void UploadCinematic(int w, int h, int cols, int rows, const uint8_t *data, int client, bool dirty)
{
	Texture *scratch = g_textureCache->getScratch(size_t(client));
	
	if (cols != scratch->getWidth() || rows != scratch->getHeight())
	{
		scratch->resize(cols, rows);
		dirty = true;
	}

	if (dirty)
	{
		const bgfx::Memory *mem = bgfx::alloc(cols * rows * 4);
		memcpy(mem->data, data, mem->size);
		scratch->update(mem, 0, 0, cols, rows);
	}
}

} // namespace main
} // namespace renderer
