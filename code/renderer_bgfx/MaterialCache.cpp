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
/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

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
	util::Strncpyz(name_, name, sizeof(name_));
	handle_ = handle;

	// If not a .skin file, load as a single shader
	if (strcmp(name + strlen(name) - 5, ".skin"))
	{
		surfaces_.resize(1);
		surfaces_[0].name[0] = 0;
		surfaces_[0].material = g_materialCache->findMaterial(name, MaterialLightmapId::None, true);
		return;
	}

	// Load and parse the skin file.
	ReadOnlyFile file(name);

	if (!file.getData())
		return;

	// 2 passes for a single memory alloc. First one counts the number of surfaces, second copies over surface data.
	size_t nSurfaces = 0;

	for (int i = 0; i < 2; i++)
	{
		size_t currentSurface = 0;

		if (i == 1)
		{
			surfaces_.resize(nSurfaces);
		}

		char *text_p = (char *)file.getData();

		while (text_p && *text_p)
		{
			// Get surface name.
			const char *token = CommaParse(&text_p);

			if (!token[0])
				break;

			if (*text_p == ',')
				text_p++;

			if (strstr(token, "tag_"))
				continue;

			// this is specifying a model
			if (strstr(token, "md3_"))
			{
				if (i == 1)
				{
					CommaParse(&text_p); // skip name
					continue;
				}

				if (nModels_ >= maxModels_)
				{
					interface::PrintWarningf("WARNING: Ignoring models in '%s', the max is %d!\n", name, (int)maxModels_);
					break;
				}

				Model &model = models_[nModels_];
				util::Strncpyz(model.type, token, sizeof(model.type));
				token = CommaParse(&text_p);
				util::Strncpyz(model.name, token, sizeof(model.name));
				nModels_++;
				continue;
			}

			//----(SA)	added
			if (strstr(token, "playerscale"))
			{
				token = CommaParse(&text_p);
				
				if (i == 0)
					scale_ = (float)atof(token); // uniform scaling for now

				continue;
			}
			//----(SA) end
		
			// Got this far, it's a surface.
			if (i == 0)
			{
				CommaParse(&text_p); // skip material
				nSurfaces++;
			}
			else
			{
				Surface &surface = surfaces_[currentSurface];
				util::Strncpyz(surface.name, token, sizeof(surface.name));
				util::ToLowerCase(surface.name); // Lowercase the surface name so skin compares are faster.
				token = CommaParse(&text_p);
				surface.material = g_materialCache->findMaterial(token, MaterialLightmapId::None, true);
				currentSurface++;
			}
		}
	}
}

Skin::Skin(const char *name, qhandle_t handle, Material *material)
{
	util::Strncpyz(name_, name, sizeof(name_));
	handle_ = handle;
	surfaces_.resize(1);
	surfaces_[0].name[0] = 0;
	surfaces_[0].material = material;
}

Material *Skin::findMaterial(const char *surfaceName)
{
	for (Surface &surface : surfaces_)
	{
		if (!strcmp(surface.name, surfaceName))
			return surface.material;
	}

	return nullptr;
}

const char *Skin::findModelName(const char *modelType) const
{
	for (const Model &model : models_)
	{
		if (!util::Stricmp(model.type, modelType))
		{
			return model.name;
		}
	}

	return nullptr;
}

MaterialCache::MaterialCache() : hashTable_(), textHashTable_()
{
	interface::Printf("Initializing Materials\n");
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
	meta::OnMaterialCreate(m.get());
	m->finish();
	m->index = (int)materials_.size();
	m->sortedIndex = (int)materials_.size();
	size_t hash = generateHash(m->name, hashTableSize_);
	m->next = hashTable_[hash];
	hashTable_[hash] = m.get();
	materials_.push_back(std::move(m));
	return hashTable_[hash];
}

Material *MaterialCache::findMaterial(const char *name, int lightmapIndex, bool mipRawImage)
{
	if (!name || !name[0])
		return defaultMaterial_;

	char strippedName[MAX_QPATH];
	util::StripExtension(name, strippedName, sizeof(strippedName));
	size_t hash = generateHash(strippedName, hashTableSize_);

	// see if the shader is already loaded
	for (Material *m = hashTable_[hash]; m; m = m->next)
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == MaterialLightmapId::None, so we
		// have to check all default shaders otherwise for every call to findMaterial
		// with that same strippedName a new default shader is created.
		if ((m->lightmapIndex == lightmapIndex || m->defaultShader) && !util::Stricmp(m->name, strippedName))
		{
			// match found
			return m;
		}
	}

	Material m(strippedName);
	m.lightmapIndex = lightmapIndex;

	// attempt to define shader from an explicit parameter file
	char *shaderText = findShaderInShaderText(strippedName);

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

	Texture *texture = g_textureCache->find(name, flags);

	if (!texture)
	{
		interface::PrintDeveloperf("Couldn't find image file for shader %s\n", name);
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
		m.stages[0].bundles[0].textures[0] = g_textureCache->getWhite();
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
			interface::PrintWarningf("WARNING: RE_RemapShader: %s shader %s not found\n", i == 0 ? "old" : "new", name);
			return;
		}
	}

	// Remap all the materials with the given name, even though they might have different lightmaps.
	char strippedName[MAX_QPATH];
	util::StripExtension(oldName, strippedName, sizeof(strippedName));
	size_t hash = generateHash(strippedName, hashTableSize_);

	for (Material *m = hashTable_[hash]; m; m = m->next)
	{
		if (util::Stricmp(m->name, strippedName) == 0)
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
		materials[1]->timeOffset = (float)atof(offsetTime);
	}
}

void MaterialCache::printMaterials() const
{
	int nStages[Material::maxStages] = {};

	for (size_t i = 0; i < materials_.size(); i++)
	{
		const Material *mat = materials_[i].get();
		bool animated = false;

		for (const MaterialStage &stage : mat->stages)
		{
			if (stage.active && stage.bundles[0].numImageAnimations > 1)
				animated = true;
		}

		interface::Printf("%4u: [%c] %s\n", (int)i, animated ? 'a' : ' ', mat->name);
		nStages[mat->numUnfoggedPasses]++;
	}

	for (int i = 1; i < Material::maxStages; i++)
	{
		if (nStages[i])
			interface::Printf("%i materials with %i stage(s)\n", nStages[i], i);
	}
}

Skin *MaterialCache::findSkin(const char *name)
{
	if (!name || !name[0])
	{
		interface::PrintDeveloperf("Empty skin name\n");
		return nullptr;
	}

	if (strlen(name) >= MAX_QPATH)
	{
		interface::PrintDeveloperf("Skin name exceeds MAX_QPATH\n");
		return nullptr;
	}

	// See if the skin is already loaded.
	for (std::unique_ptr<Skin> &skin : skins_)
	{
		if (!util::Stricmp(skin->getName(), name))
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

	Skin *result = skin.get();
	skins_.push_back(std::move(skin));
	return result;
}

Skin *MaterialCache::getSkin(qhandle_t handle)
{
	// Don't return default skin (handle 0).
	if (handle < 1 || size_t(handle) >= skins_.size())
		return nullptr;

	return skins_[handle].get();
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
	m.stages[0].bundles[0].textures[0] = g_textureCache->getDefault();
	m.stages[0].active = true;
	defaultMaterial_ = createMaterial(m);
}

void MaterialCache::scanAndLoadShaderFiles()
{
	// scan for shader files
	int numShaderFiles;
	char **shaderFiles = interface::FS_ListFiles("scripts", ".shader", &numShaderFiles);

	if (!shaderFiles || !numShaderFiles)
	{
		interface::PrintWarningf("WARNING: no shader files found\n");
		return;
	}

	numShaderFiles = std::min(numShaderFiles, (int)maxShaderFiles_);

	// load and parse shader files
	char *buffers[maxShaderFiles_] = {NULL};
	long sum = 0;

	for (int i = 0; i < numShaderFiles; i++)
	{
		char filename[MAX_QPATH];

		// look for a .mtr file first
		{
			util::Sprintf(filename, sizeof(filename), "scripts/%s", shaderFiles[i]);

			char *ext;

			if ((ext = strrchr(filename, '.')))
			{
				strcpy(ext, ".mtr");
			}

			if (interface::FS_ReadFile(filename, NULL) <= 0)
			{
				util::Sprintf(filename, sizeof(filename), "scripts/%s", shaderFiles[i]);
			}
		}
		
		interface::PrintDeveloperf("...loading '%s'\n", filename);
		long summand = interface::FS_ReadFile(filename, (uint8_t **)&buffers[i]);
		
		if (!buffers[i])
			interface::Error("Couldn't load %s", filename);
		
		// Do a simple check on the shader structure in that file to make sure one bad shader file cannot fuck up all other shaders.
		char *p = buffers[i];
		util::BeginParseSession(filename);

		while(1)
		{
			char *oldP = p;
			char *token = util::Parse(&p, true);
			
			if (!*token)
				break;

			char shaderName[MAX_QPATH];
			util::Strncpyz(shaderName, token, sizeof(shaderName));
			int shaderLine = util::GetCurrentParseLine();
			token = util::Parse(&p, true);

			if (token[0] != '{' || token[1] != '\0')
			{
				interface::PrintWarningf("WARNING: Shader file %s. Shader \"%s\" on line %d missing opening brace", filename, shaderName, shaderLine);

				if (token[0])
				{
					interface::PrintWarningf(" (found \"%s\" on line %d)", token, util::GetCurrentParseLine());
				}

				interface::PrintWarningf(". Ignoring rest of shader file.\n");
				*oldP = 0;
				break;
			}

			if (!util::SkipBracedSection(&p, 1))
			{
				interface::PrintWarningf("WARNING: Shader file %s. Shader \"%s\" on line %d missing closing brace. Ignoring rest of shader file.\n", filename, shaderName, shaderLine);
				*oldP = 0;
				break;
			}
		}
			
		if (buffers[i])
			sum += summand;		
	}

	// build single large buffer
	shaderText_.resize(sum + numShaderFiles * 2);
	shaderText_[0] = '\0';
	char *textEnd = shaderText_.data();
 
	// free in reverse order, so the temp files are all dumped
	for (int i = numShaderFiles - 1; i >= 0 ; i--)
	{
		if (!buffers[i])
			continue;

		strcat(textEnd, buffers[i]);
		strcat(textEnd, "\n");
		textEnd += strlen(textEnd);
		interface::FS_FreeReadFile((uint8_t *)buffers[i]);
	}

	util::Compress(shaderText_.data());

	// free up memory
	interface::FS_FreeListFiles(shaderFiles);

	// look for shader names
	int textHashTable_Sizes[textHashTableSize_];
	memset(textHashTable_Sizes, 0, sizeof(textHashTable_Sizes));
	int size = 0;
	char *p = shaderText_.data();

	while (1)
	{
		char *token = util::Parse(&p, true);

		if (token[0] == 0)
			break;

		size_t hash = generateHash(token, textHashTableSize_);
		textHashTable_Sizes[hash]++;
		size++;
		util::SkipBracedSection(&p, 0);
	}

	size += textHashTableSize_;
	auto hashMem = (char *)interface::Hunk_Alloc(size * sizeof(char *));

	for (int i = 0; i < textHashTableSize_; i++)
	{
		textHashTable_[i] = (char **) hashMem;
		hashMem = ((char *) hashMem) + ((textHashTable_Sizes[i] + 1) * sizeof(char *));
	}

	// look for shader names
	memset(textHashTable_Sizes, 0, sizeof(textHashTable_Sizes));
	p = shaderText_.data();

	while (1)
	{
		char *oldp = p;
		char *token = util::Parse(&p, true);

		if (token[0] == 0)
			break;

		size_t hash = generateHash(token, textHashTableSize_);
		textHashTable_[hash][textHashTable_Sizes[hash]++] = oldp;
		util::SkipBracedSection(&p, 0);
	}
}

void MaterialCache::createExternalShaders()
{
}

char *MaterialCache::findShaderInShaderText(const char *name)
{
	size_t hash = generateHash(name, textHashTableSize_);

	if (textHashTable_[hash])
	{
		for (size_t i = 0; textHashTable_[hash][i]; i++)
		{
			char *p = textHashTable_[hash][i];
			char *token = util::Parse(&p, true);
		
			if (!util::Stricmp(token, name))
				return p;
		}
	}

	char *p = shaderText_.data();

	if (!p)
		return NULL;

	// look for label
	for (;;)
	{
		char *token = util::Parse(&p, true);

		if (token[0] == 0)
			break;

		if (!util::Stricmp(token, name))
			return p;
		
		// skip the definition
		util::SkipBracedSection(&p, 0);
	}

	return NULL;
}

} // namespace renderer
