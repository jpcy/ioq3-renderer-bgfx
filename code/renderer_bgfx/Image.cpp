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

namespace renderer {

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

void Image::calculateNumMips()
{
	nMips = 1 + (int)std::floor(std::log2(std::max(width, height)));
}

void Image::allocMemory()
{
	int mipWidth = width, mipHeight = height;
	size_t memSize = 0;

	for (int i = 0; i < nMips; i++)
	{
		memSize += mipWidth * mipHeight * nComponents;
		mipWidth = std::max(1, mipWidth >> 1);
		mipHeight = std::max(1, mipHeight >> 1);
	}

	memory = bgfx::alloc((uint32_t)memSize);
}

struct ImageHandler
{
	typedef uint8_t *(*LoadFunction)(const char *filename, const uint8_t *fileBuffer, size_t fileLength, Image *image);
	typedef void (*FreeFunction)(uint8_t *data);

	char extension[MAX_QPATH];
	LoadFunction load;
	FreeFunction free;
};

static uint8_t *Stbi_LoadImage(const char *filename, const uint8_t *fileBuffer, size_t fileLength, Image *image)
{
	assert(fileBuffer);
	assert(image);
	assert(image->memory == nullptr);

	int width = 0;
	int height = 0;
	int nComponents = 0;
	uint8_t *data = stbi_load_from_memory(fileBuffer, (int)fileLength, &width, &height, &nComponents, 4);
	nComponents = 4;

	if (data == nullptr)
	{
		ri.Printf(PRINT_ALL, "Error loading image \"%s\". Reason: \"%s\"\n", filename, stbi_failure_reason());
		return nullptr;
	}

	image->width = width;
	image->height = height;
	image->nComponents = nComponents;
	return data;
}

static void Stbi_FreeImage(uint8_t *data)
{
	stbi_image_free(data);
}

static const ImageHandler imageHandlers[] =
{
	{ "bmp", &Stbi_LoadImage, Stbi_FreeImage },
	{ "jpg", &Stbi_LoadImage, Stbi_FreeImage },
	{ "jpeg", &Stbi_LoadImage, Stbi_FreeImage },
	{ "tga", &Stbi_LoadImage, Stbi_FreeImage },
	{ "png", &Stbi_LoadImage, Stbi_FreeImage }
};

static const size_t nImageHandlers = sizeof(imageHandlers) / sizeof(imageHandlers[0]);

static void FinalizeImage(Image *image, uint8_t *data, int flags)
{
	assert(image);
	assert(data);

	if (flags & Image::Flags::GenerateMipmaps)
	{
		if ((flags & Image::Flags::Picmip) && g_cvars.picmip->integer > 0)
		{
			const int oldWidth = image->width, oldHeight = image->height;
			image->width = std::max(1, image->width >> g_cvars.picmip->integer);
			image->height = std::max(1, image->height >> g_cvars.picmip->integer);
			image->calculateNumMips();
			image->allocMemory();
			stbir_resize_uint8(data, oldWidth, oldHeight, 0, image->memory->data, image->width, image->height, 0, image->nComponents);
		}
		else
		{
			image->calculateNumMips();
			image->allocMemory();
			memcpy(image->memory->data, data, image->width * image->height * image->nComponents);
		}

		int width = image->width, height = image->height;
		uint8_t *mipSource = image->memory->data;

		for (int i = 0; i < image->nMips - 1; i++)
		{
			uint8_t *mipDest = mipSource + (width * height * image->nComponents);

#if 1
			stbir_resize_uint8(mipSource, width, height, 0, mipDest, std::max(1, width >> 1), std::max(1, height >> 1), 0, image->nComponents);
#else
			// Causes lightshaft artifacts.
			bgfx::imageRgba8Downsample2x2(width, height, width * image->nComponents, mipSource, mipDest);
#endif
			mipSource = mipDest;
			width = std::max(1, width >> 1);
			height = std::max(1, height >> 1);
		}
	}
	else
	{
		image->allocMemory();
		memcpy(image->memory->data, data, image->memory->size);
	}
}

/// Load an image from a file.
/// 
/// If the filename extension is supplied, but no file exists with that extension, all the other supported extensions will be tried until one exists.
/// 
/// If the filename extension is omitted, all supported extensions will be tried until one exists.
/// 
/// In either circumstance, if a file exists but the image doesn't load (e.g. the image is corrupt), other file extensions won't be tried.
Image::Image(const char *filename, int flags)
{
	memory = nullptr;

	// Calculate the filename extension to determine which image handler to try first.
	const char *extension = util::GetExtension(filename);

	// Try the image handler that corresponds to the filename extension.
	const ImageHandler *triedHandler = nullptr;

	for (size_t i = 0; i < nImageHandlers; i++)
	{
		const ImageHandler *handler = &imageHandlers[i];

		if (!util::Stricmp(handler->extension, extension))
		{
			triedHandler = handler;
			ReadOnlyFile file(filename);

			if (!file.isValid())
				break;

			uint8_t *data = handler->load(filename, file.getData(), file.getLength(), this);

			if (!data)
				break;
			
			FinalizeImage(this, data, flags);
			handler->free(data);
			return;
		}
	}
		
	// If we got this far, either the extension was omitted, or the filename with the supplied extension doesn't exist. Either way, try loading using all/other supported extensions.
	char newFilename[MAX_QPATH];

	for (size_t i = 0; i < nImageHandlers; i++)
	{
		const ImageHandler *handler = &imageHandlers[i];

		// Don't try the image handler that corresponds to the filename extension again.
		if (handler == triedHandler)
			continue;

		util::StripExtension(filename, newFilename, sizeof(newFilename));
		util::Strcat(newFilename, sizeof(newFilename), util::VarArgs(".%s", handler->extension));

		ReadOnlyFile file(newFilename);

		if (!file.isValid())
			continue;

		uint8_t *data = handler->load(newFilename, file.getData(), file.getLength(), this);
			
		if (!data)
			continue;

		FinalizeImage(this, data, flags);
		handler->free(data);
		return;
	}
}

} // namespace renderer
