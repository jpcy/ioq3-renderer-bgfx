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

void Texture::initialize(const char *name, const Image &image, int flags, bgfx::TextureFormat::Enum format)
{
	strcpy(name_, name);
	width_ = image.width;
	height_ = image.height;
	nMips_ = image.nMips;
	flags_ = flags;
	format_ = format;

	const bgfx::Memory *mem = nullptr;

	if (image.flags & ImageFlags::DataIsBgfxMemory)
	{
		mem = (const bgfx::Memory *)image.data;
	}
	else if (image.data)
	{
		mem = bgfx::makeRef(image.data, image.dataSize, image.release);
	}

	// Create with data: immutable. Create without data: mutable, update whenever.
	handle_ = bgfx::createTexture2D(width_, height_, nMips_ > 1, 1, format_, calculateBgfxFlags(), (flags_ & TextureFlags::Mutable) ? nullptr : mem);

#ifdef _DEBUG
	bgfx::setName(handle_, name_);
#endif

	if (flags & TextureFlags::Mutable)
	{
		update(mem, 0, 0, width_, height_);
	}
}

void Texture::initialize(const char *name, bgfx::TextureHandle handle)
{
	strcpy(name_, name);
	handle_ = handle;
	flags_ = 0;
}

void Texture::resize(int width, int height)
{
	if (width == width_ && height == height_)
		return;

	bgfx::destroy(handle_);
	width_ = width;
	height_ = height;
	handle_ = bgfx::createTexture2D(width_, height_, nMips_ > 1, 1, format_, calculateBgfxFlags());

#ifdef _DEBUG
	bgfx::setName(handle_, name_);
#endif
}

void Texture::update(const bgfx::Memory *mem, int x, int y, int width, int height)
{
	bgfx::updateTexture2D(handle_, 0, 0, x, y, width, height, mem);
}

uint32_t Texture::calculateBgfxFlags() const
{
	uint32_t bgfxFlags = BGFX_TEXTURE_NONE;

	if (flags_ & TextureFlags::ClampToEdge)
	{
		bgfxFlags |= BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP;
	}

	if (main::IsMaxAnisotropyEnabled())
	{
		bgfxFlags |= BGFX_TEXTURE_MIN_ANISOTROPIC | BGFX_TEXTURE_MAG_ANISOTROPIC;
	}

	return bgfxFlags;
}

TextureCache::TextureCache() : hashTable_()
{
	// Default texture (black box with white border).
	memset(defaultImageData_, 32, defaultImageDataSize_);

	for (int x = 0; x < defaultImageSize_; x++)
	{
		*((uint32_t *)&defaultImageData_[x * defaultImageSize_ * 4]) = 0xffffffff;
		*((uint32_t *)&defaultImageData_[x * 4]) = 0xffffffff;
		*((uint32_t *)&defaultImageData_[(defaultImageSize_ - 1 + x * defaultImageSize_) * 4]) = 0xffffffff;
		*((uint32_t *)&defaultImageData_[(x + (defaultImageSize_ - 1) * defaultImageSize_) * 4]) = 0xffffffff;
	}

	defaultTexture_ = create("*default", CreateImage(defaultImageSize_, defaultImageSize_, 4, defaultImageData_), TextureFlags::Mipmap, bgfx::TextureFormat::RGBA8);

	// Noise texture.
	for (uint32_t x = 0; x < noiseImageDataSize_; x++)
	{
		auto r = uint8_t(rand() % 255);
		auto g = uint8_t(rand() % 255);
		auto b = uint8_t(rand() % 255);
		noiseImageData_[x] = (255 << 24) | (b << 16) | (g << 8) | r;
	}

	noiseTexture_ = create("*noise", CreateImage(noiseImageSize_, noiseImageSize_, 4, noiseImageData_), TextureFlags::None, bgfx::TextureFormat::RGBA8);

	// White texture.
	memset(whiteImageData_, 255, defaultImageDataSize_);
	whiteTexture_ = create("*white", CreateImage(defaultImageSize_, defaultImageSize_, 4, whiteImageData_), 0, bgfx::TextureFormat::RGBA8);

	// With overbright bits active, we need an image which is some fraction of full color, for default lightmaps, etc.
	for (int x = 0; x < defaultImageSize_ * defaultImageSize_; x++)
	{
		identityLightImageData_[x * 4 + 0] = identityLightImageData_[x * 4 + 1] = identityLightImageData_[x * 4 + 2] = uint8_t(255 * g_identityLight);
		identityLightImageData_[x * 4 + 3] = 0xff;
	}

	identityLightTexture_ = create("*identityLight", CreateImage(defaultImageSize_, defaultImageSize_, 4, identityLightImageData_), 0, bgfx::TextureFormat::RGBA8);

	// Scratch textures.
	for (size_t i = 0; i < scratchTextures_.size(); i++)
	{
		memset(scratchImageData_[i], 0, defaultImageDataSize_);
		scratchTextures_[i] = create("*scratch", CreateImage(defaultImageSize_, defaultImageSize_, 4, scratchImageData_[i]), TextureFlags::Picmip | TextureFlags::ClampToEdge, bgfx::TextureFormat::RGBA8);
	}
}

TextureCache::~TextureCache()
{
	for (size_t i = 0; i < nTextures_; i++)
	{
		bgfx::destroy(textures_[i].handle_);
	}
}

Texture *TextureCache::create(const char *name, const Image &image, int flags, bgfx::TextureFormat::Enum format)
{
	if (strlen(name) >= MAX_QPATH)
	{
		interface::Error("Texture name \"%s\" is too long", name);
	}

	if (nTextures_ == maxTextures_)
	{
		interface::Error("Exceeded max textures");
	}

	Texture *texture = &textures_[nTextures_];
	nTextures_++;
	texture->initialize(name, image, flags, format);
	hashTexture(texture);
	return texture;
}

Texture *TextureCache::create(const char *name, bgfx::TextureHandle handle)
{
	if (strlen(name) >= MAX_QPATH)
	{
		interface::Error("Texture name \"%s\" is too long", name);
	}

	if (nTextures_ == maxTextures_)
	{
		interface::Error("Exceeded max textures");
}

	Texture *texture = &textures_[nTextures_];
	nTextures_++;
	texture->initialize(name, handle);
	hashTexture(texture);
	return texture;
}

Texture *TextureCache::find(const char *name, int flags)
{
	if (!name)
		return nullptr;

	size_t hash = generateHash(name);

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
					interface::PrintDeveloperf("WARNING: reused image %s with mixed flags (%i vs %i)\n", name, t->flags_, flags);
				}
			}

			return t;
		}
	}

	// Load it from a file.
	int imageFlags = 0;

	if (flags & (TextureFlags::Mipmap | TextureFlags::Picmip))
	{
		imageFlags |= CreateImageFlags::GenerateMipmaps;
	}

	if (flags & TextureFlags::Picmip)
	{
		imageFlags |= CreateImageFlags::Picmip;
	}

	Image image = LoadImage(name, imageFlags);

	if (!image.data)
		return nullptr;

	return create(name, image, flags, bgfx::TextureFormat::RGBA8);
}

Texture *TextureCache::get(const char *name)
{
	if (!name)
		return nullptr;

	size_t hash = generateHash(name);

	for (Texture *t = hashTable_[hash]; t; t = t->next_)
	{
		if (!strcmp(name, t->name_))
		{
			return t;
		}
	}

	return nullptr;
}

void TextureCache::alias(Texture *from, Texture *to)
{
	assert(from);

	if (!to)
	{
		aliases_.erase(from);
	}
	else
	{
		aliases_.insert(std::make_pair(from, to));
	}
}

void TextureCache::hashTexture(Texture *texture)
{
	size_t hash = generateHash(texture->name_);
	texture->next_ = hashTable_[hash];
	hashTable_[hash] = texture;
}

size_t TextureCache::generateHash(const char *name) const
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
