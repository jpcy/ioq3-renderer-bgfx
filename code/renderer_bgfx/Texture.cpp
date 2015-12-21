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

Texture::Texture(const char *name, const Image &image, TextureType type, int flags, bgfx::TextureFormat::Enum format)
{
	strcpy(name_, name);
	width_ = image.width;
	height_ = image.height;
	nMips_ = image.nMips;
	type_ = type;
	flags_ = flags;
	format_ = format;
	handle_ = bgfx::createTexture2D(width_, height_, image.nMips, format_, calculateBgfxFlags(), image.memory);
}

Texture::~Texture()
{
	bgfx::destroyTexture(handle_);
}

void Texture::resize(int width, int height)
{
	if (width == width_ && height == height_)
		return;

	bgfx::destroyTexture(handle_);
	width_ = width;
	height_ = height;
	handle_ = bgfx::createTexture2D(width_, height_, nMips_, format_, calculateBgfxFlags());
}

void Texture::update(const bgfx::Memory *mem, int x, int y, int width, int height)
{
	bgfx::updateTexture2D(handle_, 0, x, y, width, height, mem);
}

uint32_t Texture::calculateBgfxFlags() const
{
	uint32_t bgfxFlags = BGFX_TEXTURE_NONE;

	if (flags_ & TextureFlags::ClampToEdge)
	{
		bgfxFlags |= BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP;
	}

	if (g_main->cvars.maxAnisotropy->integer)
	{
		bgfxFlags |= BGFX_TEXTURE_MIN_ANISOTROPIC | BGFX_TEXTURE_MAG_ANISOTROPIC;
	}

	return bgfxFlags;
}

TextureCache::TextureCache() : hashTable_()
{
	createBuiltinTextures();
}

Texture *TextureCache::createTexture(const char *name, const Image &image, TextureType type, int flags, bgfx::TextureFormat::Enum format)
{
	if (strlen(name) >= MAX_QPATH)
	{
		ri.Error(ERR_DROP, "Texture name \"%s\" is too long", name);
	}

	auto texture = std::make_unique<Texture>(name, image, type, flags, format);
	auto hash = generateHash(name);
	texture->next_ = hashTable_[hash];
	hashTable_[hash] = texture.get();
	textures_.push_back(std::move(texture));
	return hashTable_[hash];
}

Texture *TextureCache::findTexture(const char *name, TextureType type, int flags)
{
	if (!name)
		return nullptr;

	auto hash = generateHash(name);

	// See if the image is already loaded.
	for (Texture *t = hashTable_[hash]; t; t = t->next_)
	{
		if (!strcmp(name, t->name_))
		{
			// The white image can be used with any set of parms, but other mismatches are errors.
			if (strcmp(name, "*white"))
			{
				if (t->flags_ != flags)
				{
					ri.Printf(PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n", name, t->flags_, flags);
				}
			}

			return t;
		}
	}

	// Load it from a file.
	int imageFlags = 0;

	if (flags & (TextureFlags::Mipmap | TextureFlags::Picmip))
	{
		imageFlags |= Image::Flags::GenerateMipmaps;
	}

	if (flags & TextureFlags::Picmip)
	{
		imageFlags |= Image::Flags::Picmip;
	}

	Image image(name, imageFlags);

	if (!image.memory)
		return nullptr;

	return createTexture(name, image, type, flags);
}

/// The default image will be a box, to allow you to see the mapping coordinates.
void TextureCache::createBuiltinTextures()
{
	// Default texture (black box with white border).
	const int defaultSize = 16;
	Image image;
	image.width = image.height = defaultSize;
	image.nComponents = 4;
	image.allocMemory();
	Com_Memset(image.memory->data, 32, image.memory->size);

	for (int x = 0; x < defaultSize; x++)
	{
		*((uint32_t *)&image.memory->data[x * defaultSize * 4]) = 0xffffffff;
		*((uint32_t *)&image.memory->data[x * 4]) = 0xffffffff;
		*((uint32_t *)&image.memory->data[(defaultSize - 1 + x * defaultSize) * 4]) = 0xffffffff;
		*((uint32_t *)&image.memory->data[(x + (defaultSize - 1) * defaultSize) * 4]) = 0xffffffff;
	}

	defaultTexture_ = createTexture("*default", image, TextureType::ColorAlpha, TextureFlags::Mipmap);

	// White texture.
	image.width = image.height = 8;
	image.allocMemory();
	Com_Memset(image.memory->data, 255, image.memory->size);
	whiteTexture_ = createTexture("*white", image);

	// With overbright bits active, we need an image which is some fraction of full color, for default lightmaps, etc.
	image.width = image.height = defaultSize;
	image.allocMemory();

	for (int x = 0; x < defaultSize * defaultSize; x++)
	{
		image.memory->data[x * 4 + 0] = 
		image.memory->data[x * 4 + 1] = 
		image.memory->data[x * 4 + 2] = g_main->identityLightByte;
		image.memory->data[x * 4 + 3] = 0xff;
	}

	identityLightTexture_ = createTexture("*identityLight", image);

	// Scratch textures.
	image.width = image.height = defaultSize;

	for (size_t i = 0; i < scratchTextures_.size(); i++)
	{
		image.allocMemory();
		Com_Memset(image.memory->data, 0, image.memory->size);
		scratchTextures_[i] = createTexture("*scratch", image, TextureType::ColorAlpha, TextureFlags::Picmip | TextureFlags::ClampToEdge);
	}
}

size_t TextureCache::generateHash(const char *name)
{
	size_t hash = 0, i = 0;

	while (name[i] != '\0')
	{
		char letter = tolower(name[i]);
		if (letter == '.') break; // don't include extension
		if (letter == '\\') letter = '/'; // damn path names
		hash += (long)(letter)*(i + 119);
		i++;
	}

	hash &= (hashTableSize_ - 1);
	return hash;
}

} // namespace renderer
