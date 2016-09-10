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
		handle = bgfx::createTexture2D(width, height, nMips > 1, 1, format, calculateBgfxFlags(), (flags & TextureFlags::Mutable) ? nullptr : mem);

		if (flags & TextureFlags::Mutable)
		{
			update(mem, 0, 0, width, height);
		}
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
		handle = bgfx::createTexture2D(width, height, nMips > 1, 1, format, calculateBgfxFlags());
	}

	void update(const bgfx::Memory *mem, int x, int y, int width, int height)
	{
		bgfx::updateTexture2D(handle, 0, 0, x, y, width, height, mem);
	}

	uint32_t calculateBgfxFlags() const
	{
		uint32_t bgfxFlags = BGFX_TEXTURE_NONE;

		if (flags & TextureFlags::ClampToEdge)
		{
			bgfxFlags |= BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP;
		}

		if (g_cvars.maxAnisotropy.getBool())
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
	static const int defaultImageSize = 16;
	static const uint32_t defaultImageDataSize = defaultImageSize * defaultImageSize * 4;
	uint8_t defaultImageData[defaultImageDataSize];
	uint8_t whiteImageData[defaultImageDataSize];
	uint8_t identityLightImageData[defaultImageDataSize];
	TextureImpl *defaultTexture, *identityLightTexture, *whiteTexture;
	static const size_t nScratchTextures = 32;
	uint8_t scratchImageData[nScratchTextures][defaultImageDataSize];
	std::array<TextureImpl *, nScratchTextures> scratchTextures;

	TextureCache() : hashTable()
	{
		// Default texture (black box with white border).
		memset(defaultImageData, 32, defaultImageDataSize);

		for (int x = 0; x < defaultImageSize; x++)
		{
			*((uint32_t *)&defaultImageData[x * defaultImageSize * 4]) = 0xffffffff;
			*((uint32_t *)&defaultImageData[x * 4]) = 0xffffffff;
			*((uint32_t *)&defaultImageData[(defaultImageSize - 1 + x * defaultImageSize) * 4]) = 0xffffffff;
			*((uint32_t *)&defaultImageData[(x + (defaultImageSize - 1) * defaultImageSize) * 4]) = 0xffffffff;
		}

		defaultTexture = createTexture("*default", CreateImage(defaultImageSize, defaultImageSize, 4, defaultImageData), TextureFlags::Mipmap, bgfx::TextureFormat::RGBA8);

		// White texture.
		memset(whiteImageData, 255, defaultImageDataSize);
		whiteTexture = createTexture("*white", CreateImage(defaultImageSize, defaultImageSize, 4, whiteImageData), 0, bgfx::TextureFormat::RGBA8);

		// With overbright bits active, we need an image which is some fraction of full color, for default lightmaps, etc.
		for (int x = 0; x < defaultImageSize * defaultImageSize; x++)
		{
			identityLightImageData[x * 4 + 0] = identityLightImageData[x * 4 + 1] = identityLightImageData[x * 4 + 2] = uint8_t(255 * g_identityLight);
			identityLightImageData[x * 4 + 3] = 0xff;
		}

		identityLightTexture = createTexture("*identityLight", CreateImage(defaultImageSize, defaultImageSize, 4, identityLightImageData), 0, bgfx::TextureFormat::RGBA8);

		// Scratch textures.
		for (size_t i = 0; i < scratchTextures.size(); i++)
		{
			memset(scratchImageData[i], 0, defaultImageDataSize);
			scratchTextures[i] = createTexture("*scratch", CreateImage(defaultImageSize, defaultImageSize, 4, scratchImageData[i]), TextureFlags::Picmip | TextureFlags::ClampToEdge, bgfx::TextureFormat::RGBA8);
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
			interface::Error("Texture name \"%s\" is too long", name);
		}

		if (nTextures == maxTextures)
		{
			interface::Error("Exceeded max textures");
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
			interface::Error("Texture name \"%s\" is too long", name);
		}

		if (nTextures == maxTextures)
		{
			interface::Error("Exceeded max textures");
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
						interface::PrintDeveloperf("WARNING: reused image %s with mixed flags (%i vs %i)\n", name, t->flags, flags);
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

		return createTexture(name, image, flags, bgfx::TextureFormat::RGBA8);
	}

	TextureImpl *getTexture(const char *name)
	{
		if (!name)
			return nullptr;

		size_t hash = generateHash(name);

		for (TextureImpl *t = hashTable[hash]; t; t = t->next)
		{
			if (!strcmp(name, t->name))
			{
				return t;
			}
		}

		return nullptr;
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

Texture *Texture::get(const char *name)
{
	TextureImpl *impl = s_textureCache->getTexture(name);

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

int Texture::getFlags() const
{
	return s_textureCache->implFromTexture(this)->flags;
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
