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

// This is unfortunate, but the skin files aren't compatable with our normal parsing rules.
static const char *CommaParse(char **data_p)
{
	char *data = *data_p;
	static char com_token[MAX_TOKEN_CHARS];
	com_token[0] = 0;

	// make sure incoming data is valid
	if (!data)
	{
		*data_p = NULL;
		return com_token;
	}

	int len = 0;
	int c = 0;

	for (;;)
	{
		// skip whitespace
		while ((c = *data) <= ' ')
		{
			if (!c)
				break;
			data++;
		}

		c = *data;

		// skip double slash comments
		if (c == '/' && data[1] == '/')
		{
			data += 2;
			while (*data && *data != '\n')
				data++;
		}
		// skip /* */ comments
		else if (c=='/' && data[1] == '*') 
		{
			data += 2;

			while (*data && (*data != '*' || data[1] != '/')) 
				data++;

			if (*data) 
				data += 2;
		}
		else
			break;
	}

	if (c == 0)
		return "";

	// handle quoted strings
	if (c == '\"')
	{
		data++;

		for (;;)
		{
			c = *data++;

			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				*data_p = (char *) data;
				return com_token;
			}

			if (len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS - 1)
		{
			com_token[len] = c;
			len++;
		}

		data++;
		c = *data;
	} 
	while (c>32 && c != ',');

	com_token[len] = 0;
	*data_p = (char *) data;
	return com_token;
}

Skin::Skin(const char *name, qhandle_t handle)
{
	Q_strncpyz(name_, name, sizeof(name_));
	handle_ = handle;
	nSurfaces_ = 0;

	// If not a .skin file, load as a single shader
	if (strcmp(name + strlen(name) - 5, ".skin"))
	{
		nSurfaces_ = 1;
		surfaces_[0].name[0] = 0;
		surfaces_[0].material = g_main->materialCache->findMaterial(name, MaterialLightmapId::None, true);
		return;
	}

	// Load and parse the skin file.
	ReadOnlyFile file(name);

	if (!file.getData())
		return;

	char *text_p = (char *)file.getData();

	while (text_p && *text_p)
	{
		auto &surface = surfaces_[nSurfaces_];

		// Get surface name.
		auto token = CommaParse(&text_p);
		Q_strncpyz(surface.name, token, sizeof(surface.name));

		if (!token[0])
			break;

		// Lowercase the surface name so skin compares are faster.
		Q_strlwr(surface.name);

		if (*text_p == ',')
			text_p++;

		if (strstr(token, "tag_"))
			continue;
		
		// Parse the material name.
		token = CommaParse(&text_p);

		if (nSurfaces_ >= MD3_MAX_SURFACES)
		{
			ri.Printf(PRINT_WARNING, "WARNING: Ignoring surfaces in '%s', the max is %d surfaces!\n", name, MD3_MAX_SURFACES);
			break;
		}

		surface.material = g_main->materialCache->findMaterial(token, MaterialLightmapId::None, true);
		nSurfaces_++;
	}
}

Skin::Skin(const char *name, qhandle_t handle, Material *material)
{
	Q_strncpyz(name_, name, sizeof(name_));
	handle_ = handle;
	nSurfaces_ = 1;
	surfaces_[0].name[0] = 0;
	surfaces_[0].material = material;
}

Material *Skin::findMaterial(const char *surfaceName)
{
	for (auto &surface : surfaces_)
	{
		if (!strcmp(surface.name, surfaceName))
			return surface.material;
	}

	return nullptr;
}

MaterialCache::MaterialCache() : hashTable_(), textHashTable_()
{
	ri.Printf(PRINT_ALL, "Initializing Materials\n");
	createInternalShaders();
	scanAndLoadShaderFiles();
	createExternalShaders();

	// Create the default skin.
	auto skin = std::make_unique<Skin>("<default skin>", 0, defaultMaterial_);
	skins_.push_back(std::move(skin));
}

Material *MaterialCache::createMaterial(const Material &base)
{
	auto m = std::make_unique<Material>(base);
	m->finish();

	m->index = (int)materials_.size();
	m->sortedIndex = (int)materials_.size();

	auto hash = generateHash(m->name, hashTableSize_);
	m->next = hashTable_[hash];
	hashTable_[hash] = m.get();
	materials_.push_back(std::move(m));
	g_main->onMaterialCreate(hashTable_[hash]);
	return hashTable_[hash];
}

Material *MaterialCache::findMaterial(const char *name, int lightmapIndex, bool mipRawImage)
{
	if (!name || !name[0])
		return defaultMaterial_;

	char strippedName[MAX_QPATH];
	COM_StripExtension(name, strippedName, sizeof(strippedName));
	auto hash = generateHash(strippedName, hashTableSize_);

	// see if the shader is already loaded
	for (auto m = hashTable_[hash]; m; m = m->next)
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == MaterialLightmapId::None, so we
		// have to check all default shaders otherwise for every call to findMaterial
		// with that same strippedName a new default shader is created.
		if ((m->lightmapIndex == lightmapIndex || m->defaultShader) && !Q_stricmp(m->name, strippedName))
		{
			// match found
			return m;
		}
	}

	Material m(strippedName);
	m.lightmapIndex = lightmapIndex;

	// attempt to define shader from an explicit parameter file
	auto shaderText = findShaderInShaderText(strippedName);

	if (shaderText)
	{
		if (!m.parse(&shaderText))
		{
			// had errors, so use default shader
			m.defaultShader = true;
		}

		return createMaterial(m);
	}

	// if not defined in the in-memory shader descriptions, look for a single supported image file
	int flags = TextureFlags::None;

	if (mipRawImage)
	{
		flags |= TextureFlags::Mipmap | TextureFlags::Picmip;
	}
	else
	{
		flags |= TextureFlags::ClampToEdge;
	}

	auto texture = g_main->textureCache->findTexture(name, TextureType::ColorAlpha, flags);

	if (!texture)
	{
		ri.Printf(PRINT_DEVELOPER, "Couldn't find image file for shader %s\n", name);
		m.defaultShader = true;
		return createMaterial(m);
	}

	// create the default shading commands
	if (m.lightmapIndex == MaterialLightmapId::None) 
	{
		// dynamic colors at vertexes
		m.stages[0].bundles[0].textures[0] = texture;
		m.stages[0].active = true;
		m.stages[0].rgbGen = MaterialColorGen::LightingDiffuse;
	} 
	else if (m.lightmapIndex == MaterialLightmapId::Vertex)
	{
		// explicit colors at vertexes
		m.stages[0].bundles[0].textures[0] = texture;
		m.stages[0].active = true;
		m.stages[0].rgbGen = MaterialColorGen::ExactVertex;
		m.stages[0].alphaGen = MaterialAlphaGen::Skip;
	} 
	else if (m.lightmapIndex == MaterialLightmapId::StretchPic)
	{
		// GUI elements
		m.stages[0].bundles[0].textures[0] = texture;
		m.stages[0].active = true;
		m.stages[0].rgbGen = MaterialColorGen::Vertex;
		m.stages[0].alphaGen = MaterialAlphaGen::Vertex;
		m.stages[0].blendSrc = BGFX_STATE_BLEND_SRC_ALPHA;
		m.stages[0].blendDst = BGFX_STATE_BLEND_INV_SRC_ALPHA;
	} 
	else if (m.lightmapIndex == MaterialLightmapId::White)
	{
		// fullbright level
		m.stages[0].bundles[0].textures[0] = g_main->textureCache->getWhiteTexture();
		m.stages[0].active = true;
		m.stages[0].rgbGen = MaterialColorGen::IdentityLighting;

		m.stages[1].bundles[0].textures[0] = texture;
		m.stages[1].active = true;
		m.stages[1].rgbGen = MaterialColorGen::Identity;
		m.stages[1].blendSrc = BGFX_STATE_BLEND_DST_COLOR;
		m.stages[1].blendDst = BGFX_STATE_BLEND_ZERO;
	} 
	else
	{
		// two pass lightmap
		m.stages[0].bundles[0].textures[0] = world::GetLightmap(m.lightmapIndex);
		m.stages[0].bundles[0].isLightmap = true;
		m.stages[0].active = true;
		m.stages[0].rgbGen = MaterialColorGen::Identity;	// lightmaps are scaled on creation for identitylight

		m.stages[1].bundles[0].textures[0] = texture;
		m.stages[1].active = true;
		m.stages[1].rgbGen = MaterialColorGen::Identity;
		m.stages[1].blendSrc = BGFX_STATE_BLEND_DST_COLOR;
		m.stages[1].blendDst = BGFX_STATE_BLEND_ZERO;
	}

	return createMaterial(m);
}

void MaterialCache::remapMaterial(const char *oldName, const char *newName, const char *offsetTime)
{
	Material *materials[2];

	for (size_t i = 0; i < 2; i++)
	{
		const char *name = i == 0 ? oldName : newName;
		materials[i] = findMaterial(name);

		if (materials[i] == nullptr || materials[i] == defaultMaterial_)
		{
			materials[i] = findMaterial(name, 0);

			if (materials[i]->defaultShader)
				materials[i] = defaultMaterial_;
		}

		if (materials[i] == nullptr || materials[i] == defaultMaterial_)
		{
			ri.Printf(PRINT_WARNING, "WARNING: RE_RemapShader: %s shader %s not found\n", i == 0 ? "old" : "new", name);
			return;
		}
	}

	// Remap all the materials with the given name, even though they might have different lightmaps.
	char strippedName[MAX_QPATH];
	COM_StripExtension(oldName, strippedName, sizeof(strippedName));
	auto hash = generateHash(strippedName, hashTableSize_);

	for (auto m = hashTable_[hash]; m; m = m->next)
	{
		if (Q_stricmp(m->name, strippedName) == 0)
		{
			if (m != materials[1])
			{
				m->remappedShader = materials[1];
			}
			else
			{
				m->remappedShader = nullptr;
			}
		}
	}

	if (offsetTime)
	{
		materials[1]->timeOffset = atof(offsetTime);
	}
}

Skin *MaterialCache::findSkin(const char *name)
{
	if (!name || !name[0])
	{
		ri.Printf(PRINT_DEVELOPER, "Empty skin name\n");
		return nullptr;
	}

	if (strlen(name) >= MAX_QPATH)
	{
		ri.Printf(PRINT_DEVELOPER, "Skin name exceeds MAX_QPATH\n");
		return nullptr;
	}

	// See if the skin is already loaded.
	for (auto &skin : skins_)
	{
		if (!Q_stricmp(skin->getName(), name))
		{
			if (!skin->hasSurfaces())
				return nullptr;
			
			return skin.get();
		}
	}

	// Create a new skin.
	auto skin = std::make_unique<Skin>(name, (qhandle_t)skins_.size());

	// Never let a skin have 0 surfaces.
	if (!skin->hasSurfaces())
		return nullptr; // Use default skin.

	auto result = skin.get();
	skins_.push_back(std::move(skin));
	return result;
}

size_t MaterialCache::generateHash(const char *fname, size_t size)
{
	size_t hash = 0;
	int i = 0;

	while (fname[i] != '\0')
	{
		char letter = tolower(fname[i]);
		if (letter =='.') break; // don't include extension
		if (letter =='\\') letter = '/'; // damn path names
		if (letter == PATH_SEP) letter = '/'; // damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}

	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size-1);
	return hash;
}

void MaterialCache::createInternalShaders()
{
	Material m("<default>");
	m.stages[0].bundles[0].textures[0] = g_main->textureCache->getDefaultTexture();
	m.stages[0].active = true;
	defaultMaterial_ = createMaterial(m);
}

void MaterialCache::scanAndLoadShaderFiles()
{
	// scan for shader files
	int numShaderFiles;
	auto shaderFiles = ri.FS_ListFiles("scripts", ".shader", &numShaderFiles);

	if (!shaderFiles || !numShaderFiles)
	{
		ri.Printf(PRINT_WARNING, "WARNING: no shader files found\n");
		return;
	}

	numShaderFiles = MIN(numShaderFiles, maxShaderFiles_);

	// load and parse shader files
	char *buffers[maxShaderFiles_] = {NULL};
	long sum = 0;

	for (int i = 0; i < numShaderFiles; i++)
	{
		char filename[MAX_QPATH];

		// look for a .mtr file first
		{
			Com_sprintf(filename, sizeof(filename), "scripts/%s", shaderFiles[i]);

			char *ext;

			if ((ext = strrchr(filename, '.')))
			{
				strcpy(ext, ".mtr");
			}

			if (ri.FS_ReadFile(filename, NULL) <= 0)
			{
				Com_sprintf(filename, sizeof(filename), "scripts/%s", shaderFiles[i]);
			}
		}
		
		ri.Printf(PRINT_DEVELOPER, "...loading '%s'\n", filename);
		long summand = ri.FS_ReadFile(filename, (void **)&buffers[i]);
		
		if (!buffers[i])
			ri.Error(ERR_DROP, "Couldn't load %s", filename);
		
		// Do a simple check on the shader structure in that file to make sure one bad shader file cannot fuck up all other shaders.
		char *p = buffers[i];
		COM_BeginParseSession(filename);

		while(1)
		{
			auto token = COM_ParseExt(&p, qtrue);
			
			if (!*token)
				break;

			char shaderName[MAX_QPATH];
			Q_strncpyz(shaderName, token, sizeof(shaderName));
			int shaderLine = COM_GetCurrentParseLine();

			token = COM_ParseExt(&p, qtrue);

			if (token[0] != '{' || token[1] != '\0')
			{
				ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing opening brace", filename, shaderName, shaderLine);

				if (token[0])
				{
					ri.Printf(PRINT_WARNING, " (found \"%s\" on line %d)", token, COM_GetCurrentParseLine());
				}

				ri.Printf(PRINT_WARNING, ".\n");
				ri.FS_FreeFile(buffers[i]);
				buffers[i] = NULL;
				break;
			}

			if (!SkipBracedSection(&p, 1))
			{
				ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing closing brace.\n", filename, shaderName, shaderLine);
				ri.FS_FreeFile(buffers[i]);
				buffers[i] = NULL;
				break;
			}
		}
			
		if (buffers[i])
			sum += summand;		
	}

	// build single large buffer
	s_shaderText = (char *)ri.Hunk_Alloc(sum + numShaderFiles*2, h_low);
	s_shaderText[0] = '\0';
	char *textEnd = s_shaderText;
 
	// free in reverse order, so the temp files are all dumped
	for (int i = numShaderFiles - 1; i >= 0 ; i--)
	{
		if (!buffers[i])
			continue;

		strcat(textEnd, buffers[i]);
		strcat(textEnd, "\n");
		textEnd += strlen(textEnd);
		ri.FS_FreeFile(buffers[i]);
	}

	COM_Compress(s_shaderText);

	// free up memory
	ri.FS_FreeFileList(shaderFiles);

	// look for shader names
	int textHashTable_Sizes[textHashTableSize_];
	Com_Memset(textHashTable_Sizes, 0, sizeof(textHashTable_Sizes));
	int size = 0;
	char *p = s_shaderText;

	while (1)
	{
		auto token = COM_ParseExt(&p, qtrue);

		if (token[0] == 0)
			break;

		auto hash = generateHash(token, textHashTableSize_);
		textHashTable_Sizes[hash]++;
		size++;
		SkipBracedSection(&p, 0);
	}

	size += textHashTableSize_;
	auto hashMem = (char *)ri.Hunk_Alloc(size * sizeof(char *), h_low);

	for (int i = 0; i < textHashTableSize_; i++)
	{
		textHashTable_[i] = (char **) hashMem;
		hashMem = ((char *) hashMem) + ((textHashTable_Sizes[i] + 1) * sizeof(char *));
	}

	// look for shader names
	Com_Memset(textHashTable_Sizes, 0, sizeof(textHashTable_Sizes));
	p = s_shaderText;

	while (1)
	{
		char *oldp = p;
		auto token = COM_ParseExt(&p, qtrue);

		if (token[0] == 0)
			break;

		auto hash = generateHash(token, textHashTableSize_);
		textHashTable_[hash][textHashTable_Sizes[hash]++] = oldp;
		SkipBracedSection(&p, 0);
	}
}

void MaterialCache::createExternalShaders()
{
}

char *MaterialCache::findShaderInShaderText(const char *name)
{
	auto hash = generateHash(name, textHashTableSize_);

	if (textHashTable_[hash])
	{
		for (size_t i = 0; textHashTable_[hash][i]; i++)
		{
			auto p = textHashTable_[hash][i];
			auto token = COM_ParseExt(&p, qtrue);
		
			if (!Q_stricmp(token, name))
				return p;
		}
	}

	auto p = s_shaderText;

	if (!p)
		return NULL;

	// look for label
	for (;;)
	{
		auto token = COM_ParseExt(&p, qtrue);

		if (token[0] == 0)
			break;

		if (!Q_stricmp(token, name))
			return p;
		
		// skip the definition
		SkipBracedSection(&p, 0);
	}

	return NULL;
}

} // namespace renderer
