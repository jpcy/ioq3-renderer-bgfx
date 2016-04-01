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

struct TextureImpl
{
	char name[MAX_QPATH];
	int flags;
	int width, height;
	int nMips;
	bgfx::TextureFormat::Enum format;
	bgfx::TextureHandle handle;
	TextureImpl *next;

	void initialize(const char *name, const Image &image, int flags, bgfx::TextureFormat::Enum format)
	{
		strcpy(this->name, name);
		width = image.width;
		height = image.height;
		nMips = image.nMips;
		this->flags = flags;
		this->format = format;
		handle = bgfx::createTexture2D(width, height, nMips, format, calculateBgfxFlags(), image.memory);
	}

	void initialize(const char *name, bgfx::TextureHandle handle)
	{
		strcpy(this->name, name);
		this->handle = handle;
		flags = 0;
	}

	void resize(int width, int height)
	{
		if (width == this->width && height == this->height)
			return;

		bgfx::destroyTexture(handle);
		this->width = width;
		this->height = height;
		handle = bgfx::createTexture2D(width, height, nMips, format, calculateBgfxFlags());
	}

	void update(const bgfx::Memory *mem, int x, int y, int width, int height)
	{
		bgfx::updateTexture2D(handle, 0, x, y, width, height, mem);
	}

	uint32_t calculateBgfxFlags() const
	{
		uint32_t bgfxFlags = BGFX_TEXTURE_NONE;

		if (flags & TextureFlags::ClampToEdge)
		{
			bgfxFlags |= BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP;
		}

		if (g_cvars.maxAnisotropy->integer)
		{
			bgfxFlags |= BGFX_TEXTURE_MIN_ANISOTROPIC | BGFX_TEXTURE_MAG_ANISOTROPIC;
		}

		return bgfxFlags;
	}
};

struct TextureCache
{
	static const size_t maxTextures = 2048;
	Texture textures[maxTextures];
	TextureImpl textureImpls[maxTextures];
	size_t nTextures = 0;
	static const size_t hashTableSize = 1024;
	TextureImpl *hashTable[hashTableSize];
	TextureImpl *defaultTexture, *identityLightTexture, *whiteTexture;
	std::array<TextureImpl *, 32> scratchTextures;

	TextureCache() : hashTable()
	{
		// Default texture (black box with white border).
		const int defaultSize = 16;
		Image image;
		image.width = image.height = defaultSize;
		image.nComponents = 4;
		image.allocMemory();
		memset(image.memory->data, 32, image.memory->size);

		for (int x = 0; x < defaultSize; x++)
		{
			*((uint32_t *)&image.memory->data[x * defaultSize * 4]) = 0xffffffff;
			*((uint32_t *)&image.memory->data[x * 4]) = 0xffffffff;
			*((uint32_t *)&image.memory->data[(defaultSize - 1 + x * defaultSize) * 4]) = 0xffffffff;
			*((uint32_t *)&image.memory->data[(x + (defaultSize - 1) * defaultSize) * 4]) = 0xffffffff;
		}

		defaultTexture = createTexture("*default", image, TextureFlags::Mipmap, bgfx::TextureFormat::RGBA8);

		// White texture.
		image.width = image.height = 8;
		image.allocMemory();
		memset(image.memory->data, 255, image.memory->size);
		whiteTexture = createTexture("*white", image, 0, bgfx::TextureFormat::RGBA8);

		// With overbright bits active, we need an image which is some fraction of full color, for default lightmaps, etc.
		image.width = image.height = defaultSize;
		image.allocMemory();
		const uint8_t identityLightByte = uint8_t(255 * g_identityLight);

		for (int x = 0; x < defaultSize * defaultSize; x++)
		{
			image.memory->data[x * 4 + 0] = image.memory->data[x * 4 + 1] = image.memory->data[x * 4 + 2] = identityLightByte;
			image.memory->data[x * 4 + 3] = 0xff;
		}

		identityLightTexture = createTexture("*identityLight", image, 0, bgfx::TextureFormat::RGBA8);

		// Scratch textures.
		image.width = image.height = defaultSize;

		for (size_t i = 0; i < scratchTextures.size(); i++)
		{
			image.allocMemory();
			memset(image.memory->data, 0, image.memory->size);
			scratchTextures[i] = createTexture("*scratch", image, TextureFlags::Picmip | TextureFlags::ClampToEdge, bgfx::TextureFormat::RGBA8);
		}
	}

	~TextureCache()
	{
		for (size_t i = 0; i < nTextures; i++)
		{
			bgfx::destroyTexture(textureImpls[i].handle);
		}
	}

	TextureImpl *createTexture(const char *name, const Image &image, int flags, bgfx::TextureFormat::Enum format)
	{
		if (strlen(name) >= MAX_QPATH)
		{
			ri.Error(ERR_DROP, "Texture name \"%s\" is too long", name);
		}

		if (nTextures == maxTextures)
		{
			ri.Error(ERR_DROP, "Exceeded max textures");
		}

		TextureImpl *texture = &textureImpls[nTextures];
#ifdef _DEBUG
		strcpy(textures[nTextures].name, name);
#endif
		nTextures++;
		texture->initialize(name, image, flags, format);
		hashTexture(texture);
		return texture;
	}

	TextureImpl *createTexture(const char *name, bgfx::TextureHandle handle)
	{
		if (strlen(name) >= MAX_QPATH)
		{
			ri.Error(ERR_DROP, "Texture name \"%s\" is too long", name);
		}

		if (nTextures == maxTextures)
		{
			ri.Error(ERR_DROP, "Exceeded max textures");
	}

		TextureImpl *texture = &textureImpls[nTextures];
#ifdef _DEBUG
		strcpy(textures[nTextures].name, name);
#endif
		nTextures++;
		texture->initialize(name, handle);
		hashTexture(texture);
		return texture;
	}

	TextureImpl *findTexture(const char *name, int flags)
	{
		if (!name)
			return nullptr;

		size_t hash = generateHash(name);

		// See if the image is already loaded.
		for (TextureImpl *t = hashTable[hash]; t; t = t->next)
		{
			if (!strcmp(name, t->name))
			{
				// The white image can be used with any set of parms, but other mismatches are errors.
				if (strcmp(name, "*white"))
				{
					if (t->flags != flags)
					{
						ri.Printf(PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n", name, t->flags, flags);
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

		return createTexture(name, image, flags, bgfx::TextureFormat::RGBA8);
	}

	void hashTexture(TextureImpl *texture)
	{
		size_t hash = generateHash(texture->name);
		texture->next = hashTable[hash];
		hashTable[hash] = texture;
	}

	size_t generateHash(const char *name) const
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

		hash &= (hashTableSize - 1);
		return hash;
	}

	Texture *textureFromImpl(TextureImpl *impl)
	{
		assert(impl);
		return &textures[size_t(impl - textureImpls)];
	}

	TextureImpl *implFromTexture(Texture *texture)
	{
		assert(texture);
		return &textureImpls[size_t(texture - textures)];
	}

	const TextureImpl *implFromTexture(const Texture *texture)
	{
		assert(texture);
		return &textureImpls[size_t(texture - textures)];
	}
};

static std::unique_ptr<TextureCache> s_textureCache;

void Texture::initializeCache()
{
	s_textureCache = std::make_unique<TextureCache>();
}

void Texture::shutdownCache()
{
	s_textureCache.reset();
}

Texture *Texture::create(const char *name, const Image &image, int flags, bgfx::TextureFormat::Enum format)
{
	return s_textureCache->textureFromImpl(s_textureCache->createTexture(name, image, flags, format));
}

Texture *Texture::create(const char *name, bgfx::TextureHandle handle)
{
	return s_textureCache->textureFromImpl(s_textureCache->createTexture(name, handle));
}

Texture *Texture::find(const char *name, int flags)
{
	TextureImpl *impl = s_textureCache->findTexture(name, flags);

	if (!impl)
		return nullptr;

	return s_textureCache->textureFromImpl(impl);
}

const Texture *Texture::getDefault()
{
	return s_textureCache->textureFromImpl(s_textureCache->defaultTexture);
}

const Texture *Texture::getIdentityLight()
{
	return s_textureCache->textureFromImpl(s_textureCache->identityLightTexture);
}

const Texture *Texture::getWhite()
{
	return s_textureCache->textureFromImpl(s_textureCache->whiteTexture);
}

Texture *Texture::getScratch(size_t index)
{
	return s_textureCache->textureFromImpl(s_textureCache->scratchTextures[index]);
}

void Texture::resize(int width, int height)
{
	s_textureCache->implFromTexture(this)->resize(width, height);
}

void Texture::update(const bgfx::Memory *mem, int x, int y, int width, int height)
{
	s_textureCache->implFromTexture(this)->update(mem, x, y, width, height);
}

bgfx::TextureHandle Texture::getHandle() const
{
	return s_textureCache->implFromTexture(this)->handle;
}

const char *Texture::getName() const
{
	return s_textureCache->implFromTexture(this)->name;
}

int Texture::getWidth() const
{
	return s_textureCache->implFromTexture(this)->width;
}

int Texture::getHeight() const
{
	return s_textureCache->implFromTexture(this)->height;
}

} // namespace renderer
