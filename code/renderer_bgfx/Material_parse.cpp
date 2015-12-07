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

template<typename Value>
class NameToValueConverter
{
public:
	struct Pair
	{
		const char *name;
		Value value;
	};

	bool convert(const char *name, Value *value, std::initializer_list<Pair> pairs)
	{
		for (auto &p : pairs)
		{
			if (Q_stricmp(p.name, name) == 0)
			{
				if (value)
					*value = p.value;

				return true;
			}
		}

		return false;
	}
};

struct infoParm_t
{
	const char *name;
	unsigned int clearSolid, surfaceFlags, contents;
};

// this table is also present in q3map
static infoParm_t infoParms[] =
{
	// server relevant contents
	{"water",		1,	0,	CONTENTS_WATER },
	{"slime",		1,	0,	CONTENTS_SLIME },		// mildly damaging
	{"lava",		1,	0,	CONTENTS_LAVA },		// very damaging
	{"playerclip",	1,	0,	CONTENTS_PLAYERCLIP },
	{"monsterclip",	1,	0,	CONTENTS_MONSTERCLIP },
	{"nodrop",		1,	0,	CONTENTS_NODROP },		// don't drop items or leave bodies (death fog, lava, etc)
	{"nonsolid",	1,	SURF_NONSOLID,	0},						// clears the solid flag

	// utility relevant attributes
	{"origin",		1,	0,	CONTENTS_ORIGIN },		// center of rotating brushes
	{"trans",		0,	0,	CONTENTS_TRANSLUCENT },	// don't eat contained surfaces
	{"detail",		0,	0,	CONTENTS_DETAIL },		// don't include in structural bsp
	{"structural",	0,	0,	CONTENTS_STRUCTURAL },	// force into structural bsp even if trnas
	{"areaportal",	1,	0,	CONTENTS_AREAPORTAL },	// divides areas
	{"clusterportal", 1,0,  CONTENTS_CLUSTERPORTAL },	// for bots
	{"donotenter",  1,  0,  CONTENTS_DONOTENTER },		// for bots

	{"fog",			1,	0,	CONTENTS_FOG},			// carves surfaces entering
	{"sky",			0,	SURF_SKY,		0 },		// emit light from an environment map
	{"lightfilter",	0,	SURF_LIGHTFILTER, 0 },		// filter light going through it
	{"alphashadow",	0,	SURF_ALPHASHADOW, 0 },		// test light on a per-pixel basis
	{"hint",		0,	SURF_HINT,		0 },		// use as a primary splitter

	// server attributes
	{"slick",		0,	SURF_SLICK,		0 },
	{"noimpact",	0,	SURF_NOIMPACT,	0 },		// don't make impact explosions or marks
	{"nomarks",		0,	SURF_NOMARKS,	0 },		// don't make impact marks, but still explode
	{"ladder",		0,	SURF_LADDER,	0 },
	{"nodamage",	0,	SURF_NODAMAGE,	0 },
	{"metalsteps",	0,	SURF_METALSTEPS,0 },
	{"flesh",		0,	SURF_FLESH,		0 },
	{"nosteps",		0,	SURF_NOSTEPS,	0 },

	// drawsurf attributes
	{"nodraw",		0,	SURF_NODRAW,	0 },	// don't generate a drawsurface (or a lightmap)
	{"pointlight",	0,	SURF_POINTLIGHT, 0 },	// sample lighting at vertexes
	{"nolightmap",	0,	SURF_NOLIGHTMAP,0 },	// don't generate a lightmap
	{"nodlight",	0,	SURF_NODLIGHT, 0 },		// don't ever add dynamic lights
	{"dust",		0,	SURF_DUST, 0}			// leave a dust trail when walking on this surface
};

bool Material::parse(char **text)
{
	auto token = COM_ParseExt(text, qtrue);

	if (token[0] != '{')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, name);
		return false;
	}

	int stageIndex = 0;

	for (;;)
	{
		token = COM_ParseExt(text, qtrue);

		if (!token[0])
		{
			ri.Printf(PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", name);
			return false;
		}

		// end of shader definition
		if (token[0] == '}')
		{
			break;
		}
		// stage definition
		else if (token[0] == '{')
		{
			if (stageIndex >= maxStages)
			{
				ri.Printf(PRINT_WARNING, "WARNING: too many stages in shader %s (max is %i)\n", name, maxStages);
				return false;
			}

			if (!parseStage(&stages[stageIndex], text))
				return false;

			stages[stageIndex].active = true;
			stageIndex++;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if (!Q_stricmpn(token, "qer", 3))
		{
			SkipRestOfLine(text);
		}
		// sun parms
		else if (!Q_stricmp(token, "q3map_sun") || !Q_stricmp(token, "q3map_sunExt") || !Q_stricmp(token, "q3gl2_sun"))
		{
			bool isGL2Sun = false;

			if (!Q_stricmp(token, "q3gl2_sun"))
			{
				isGL2Sun = true;
				g_main->sunShadows = qtrue;
			}

			token = COM_ParseExt(text, qfalse);
			g_main->sunLight[0] = atof(token);
			token = COM_ParseExt(text, qfalse);
			g_main->sunLight[1] = atof(token);
			token = COM_ParseExt(text, qfalse);
			g_main->sunLight[2] = atof(token);
			
			token = COM_ParseExt(text, qfalse);
			g_main->sunLight.normalize();
			g_main->sunLight *= atof(token);

			token = COM_ParseExt(text, qfalse);
			const float a = atof(token) / (180 * M_PI);

			token = COM_ParseExt(text, qfalse);
			float b = atof(token) / (180 * M_PI);

			g_main->sunDirection[0] = cos(a) * cos(b);
			g_main->sunDirection[1] = sin(a) * cos(b);
			g_main->sunDirection[2] = sin(b);

			if (isGL2Sun)
			{
				token = COM_ParseExt(text, qfalse);
				g_main->mapLightScale = atof(token);

				token = COM_ParseExt(text, qfalse);
				g_main->sunShadowScale = atof(token);
			}

			SkipRestOfLine(text);
		}
		// tonemap parms
		else if (!Q_stricmp(token, "q3gl2_tonemap"))
		{
			token = COM_ParseExt(text, qfalse);
			g_main->toneMinAvgMaxLevel[0] = atof(token);
			token = COM_ParseExt(text, qfalse);
			g_main->toneMinAvgMaxLevel[1] = atof(token);
			token = COM_ParseExt(text, qfalse);
			g_main->toneMinAvgMaxLevel[2] = atof(token);

			token = COM_ParseExt(text, qfalse);
			g_main->autoExposureMinMax[0] = atof(token);
			token = COM_ParseExt(text, qfalse);
			g_main->autoExposureMinMax[1] = atof(token);

			SkipRestOfLine(text);
		}
		else if (!Q_stricmp(token, "deformVertexes"))
		{
			if (numDeforms == maxDeforms)
			{
				ri.Printf(PRINT_WARNING, "WARNING: max deforms in '%s'\n", name);
				continue;
			}

			deforms[numDeforms] = parseDeform(text);
			numDeforms++;
		}
		else if (!Q_stricmp(token, "tesssize"))
		{
			SkipRestOfLine(text);
		}
		else if (!Q_stricmp(token, "clampTime")) 
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0])
			{
				clampTime = atof(token);
			}
		}
		// skip stuff that only the q3map needs
		else if (!Q_stricmpn(token, "q3map", 5))
		{
			SkipRestOfLine(text);
		}
		// skip stuff that only q3map or the server needs
		else if (!Q_stricmp(token, "surfaceParm"))
		{
			const size_t nInfoParms = ARRAY_LEN(infoParms);
			token = COM_ParseExt(text, qfalse);

			for (size_t i = 0; i < nInfoParms; i++)
			{
				if (!Q_stricmp(token, infoParms[i].name))
				{
					surfaceFlags |= infoParms[i].surfaceFlags;
					contentFlags |= infoParms[i].contents;
					break;
				}
			}
		}
		// no mip maps
		else if (!Q_stricmp(token, "nomipmaps"))
		{
			noMipMaps = true;
			noPicMip = true;
		}
		// no picmip adjustment
		else if (!Q_stricmp(token, "nopicmip"))
		{
			noPicMip = true;
		}
		// polygonOffset
		else if (!Q_stricmp(token, "polygonOffset"))
		{
			polygonOffset = true;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if (!Q_stricmp(token, "entityMergable"))
		{
			entityMergable = true;
		}
		// fogParms
		else if (!Q_stricmp(token, "fogParms"))
		{
			bool parsedColor;
			fogParms.color = parseVector(text, &parsedColor);

			if (!parsedColor)
			{
				return false;
			}

			token = COM_ParseExt(text, qfalse);

			if (!token[0]) 
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", name);
				continue;
			}

			fogParms.depthForOpaque = atof(token);

			// skip any old gradient directions
			SkipRestOfLine(text);
		}
		// portal
		else if (!Q_stricmp(token, "portal"))
		{
			sort = MaterialSort::Portal;
			isPortal = true;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if (!Q_stricmp(token, "skyparms"))
		{
			parseSkyParms(text);
		}
		// light <value> determines flaring in q3map, not needed here
		else if (!Q_stricmp(token, "light")) 
		{
			COM_ParseExt(text, qfalse);
		}
		// cull <face>
		else if (!Q_stricmp(token, "cull")) 
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", name);
			}
			else if (!Q_stricmp(token, "none") || !Q_stricmp(token, "twosided") || !Q_stricmp(token, "disable"))
			{
				cullType = MaterialCullType::TwoSided;
			}
			else if (!Q_stricmp(token, "back") || !Q_stricmp(token, "backside") || !Q_stricmp(token, "backsided"))
			{
				cullType = MaterialCullType::BackSided;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, name);
			}
		}
		// sort
		else if (!Q_stricmp(token, "sort"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", name);
				continue;
			}

			sort = sortFromName(token);
		}
		else
		{
			ri.Printf(PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, name);
			return false;
		}
	}

	// ignore shaders that don't have any stages, unless it is a sky or fog
	if (stageIndex == 0 && !isSky && !(contentFlags & CONTENTS_FOG))
	{
		return false;
	}

	explicitlyDefined = true;
	return true;
}

vec3 Material::parseVector(char **text, bool *result) const
{
	vec3 v;

	// FIXME: spaces are currently required after parens, should change parseext...
	auto token = COM_ParseExt(text, qfalse);

	if (strcmp(token, "("))
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", name);

		if (result)
			*result = false;

		return v;
	}

	for (size_t i = 0; i < 3; i++)
	{
		token = COM_ParseExt(text, qfalse);

		if (!token[0])
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", name);

			if (result)
				*result = false;

			return v;
		}

		v[i] = atof(token);
	}

	token = COM_ParseExt(text, qfalse);

	if (strcmp(token, ")"))
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", name);

		if (result)
			*result = false;

		return v;
	}

	if (result)
		*result = true;

	return v;
}

bool Material::parseStage(MaterialStage *stage, char **text)
{
	bool depthWriteExplicit = false;

	stage->active = true;

	for (;;)
	{
		auto token = COM_ParseExt(text, qtrue);

		if (!token[0])
		{
			ri.Printf(PRINT_WARNING, "WARNING: no matching '}' found\n");
			return false;
		}

		if (token[0] == '}')
			break;
		
		// map <name>
		if (!Q_stricmp(token, "map"))
		{
			token = COM_ParseExt(text, qfalse);

			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", name);
				return false;
			}

			if (!Q_stricmp(token, "$whiteimage"))
			{
				stage->bundles[0].textures[0] = g_main->textureCache->getWhiteTexture();
			}
			else if (!Q_stricmp(token, "$lightmap"))
			{
				stage->bundles[0].isLightmap = true;
				stage->bundles[0].textures[0] = g_main->textureCache->getWhiteTexture();

				if (g_main->world.get())
				{
					auto lightmap = g_main->world->getLightmap(lightmapIndex);
					stage->bundles[0].textures[0] = lightmap ? lightmap : g_main->textureCache->getWhiteTexture();
				}
			}
			else if (!Q_stricmp(token, "$deluxemap"))
			{
				/*if (!tr.worldDeluxeMapping)
				{
					ri.Printf(PRINT_WARNING, "WARNING: shader '%s' wants a deluxe map in a map compiled without them\n", name);
					return qfalse;
				}*/

				stage->bundles[0].isLightmap = true;
				stage->bundles[0].textures[0] = g_main->textureCache->getWhiteTexture();

				/*if (lightmapIndex < 0)
				{
					stage->bundles[0].textures[0] = g_main->textureCache->getWhiteTexture();
				}
				else
				{
					stage->bundles[0].textures[0] = tr.deluxemaps[lightmapIndex];
				}*/
			}
			else
			{
				TextureType type = TextureType::ColorAlpha;
				int flags = TextureFlags::None;

				if (!noMipMaps)
					flags |= TextureFlags::Mipmap;

				if (!noPicMip)
					flags |= TextureFlags::Picmip;

				if (stage->type == MaterialStageType::NormalMap || stage->type == MaterialStageType::NormalParallaxMap)
				{
					type = TextureType::Normal;
					flags |= TextureFlags::NoLightScale;

					if (stage->type == MaterialStageType::NormalParallaxMap)
						type = TextureType::NormalHeight;
				}
				/*else
				{
					if (r_genNormalMaps->integer)
						flags |= TextureFlags::GenNormalMap;
				}*/

				stage->bundles[0].textures[0] = g_main->textureCache->findTexture(token, type, flags);

				if (!stage->bundles[0].textures[0])
				{
					ri.Printf(PRINT_WARNING, "WARNING: could not find texture '%s' in shader '%s'\n", token, name);
					return false;
				}
			}
		}
		// clampmap <name>
		else if (!Q_stricmp(token, "clampmap"))
		{
			TextureType type = TextureType::ColorAlpha;
			int flags = TextureFlags::ClampToEdge;

			token = COM_ParseExt(text, qfalse);
			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", name);
				return qfalse;
			}

			if (!noMipMaps)
				flags |= TextureFlags::Mipmap;

			if (!noPicMip)
				flags |= TextureFlags::Picmip;

			if (stage->type == MaterialStageType::NormalMap || stage->type == MaterialStageType::NormalParallaxMap)
			{
				type = TextureType::Normal;
				flags |= TextureFlags::NoLightScale;

				if (stage->type == MaterialStageType::NormalParallaxMap)
					type = TextureType::NormalHeight;
			}
			/*else
			{
				if (r_genNormalMaps->integer)
					flags |= TextureFlags::GenNormalMap;
			}*/

			stage->bundles[0].textures[0] = g_main->textureCache->findTexture(token, type, flags);

			if (!stage->bundles[0].textures[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: could not find texture '%s' in shader '%s'\n", token, name);
				return false;
			}
		}
		// animMap <frequency> <image1> .... <imageN>
		else if (!Q_stricmp(token, "animMap"))
		{
			token = COM_ParseExt(text, qfalse);

			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'animMap' keyword in shader '%s'\n", name);
				return false;
			}

			stage->bundles[0].imageAnimationSpeed = atof(token);

			// parse up to MaterialTextureBundle::maxImageAnimations animations
			for (;;)
			{
				token = COM_ParseExt(text, qfalse);

				if (!token[0])
					break;

				const int num = stage->bundles[0].numImageAnimations;

				if (num < MaterialTextureBundle::maxImageAnimations)
				{
					int flags = TextureFlags::None;

					if (!noMipMaps)
						flags |= TextureFlags::Mipmap;

					if (!noPicMip)
						flags |= TextureFlags::Picmip;

					stage->bundles[0].textures[num] = g_main->textureCache->findTexture(token, TextureType::ColorAlpha, flags);

					if (!stage->bundles[0].textures[num])
					{
						ri.Printf(PRINT_WARNING, "WARNING: could not find texture '%s' in shader '%s'\n", token, name);
						return false;
					}

					stage->bundles[0].numImageAnimations++;
				}
			}
		}
		else if (!Q_stricmp(token, "videoMap"))
		{
			token = COM_ParseExt(text, qfalse);

			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", name);
				return false;
			}

			stage->bundles[0].videoMapHandle = ri.CIN_PlayCinematic(token, 0, 0, 256, 256, CIN_loop | CIN_silent | CIN_shader);

			if (stage->bundles[0].videoMapHandle != -1)
			{
				stage->bundles[0].isVideoMap = true;
				stage->bundles[0].textures[0] = g_main->textureCache->getScratchTextures()[stage->bundles[0].videoMapHandle];
			}
		}
		// alphafunc <func>
		else if (!Q_stricmp(token, "alphaFunc"))
		{
			token = COM_ParseExt(text, qfalse);

			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", name);
				return false;
			}


			stage->alphaTest = alphaTestFromName(token);
		}
		// depthFunc <func>
		else if (!Q_stricmp(token, "depthfunc"))
		{
			token = COM_ParseExt(text, qfalse);

			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", name);
				return false;
			}

			if (!Q_stricmp(token, "lequal"))
			{
				stage->depthTestBits = BGFX_STATE_DEPTH_TEST_LEQUAL;
			}
			else if (!Q_stricmp(token, "equal"))
			{
				stage->depthTestBits = BGFX_STATE_DEPTH_TEST_EQUAL;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, name);
			}
		}
		// detail
		else if (!Q_stricmp(token, "detail"))
		{
			stage->isDetail = qtrue;
		}
		// blendfunc <srcFactor> <dstFactor> or blendfunc <add|filter|blend>
		else if (!Q_stricmp(token, "blendfunc"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", name);
				continue;
			}

			// check for "simple" blends first
			if (!Q_stricmp(token, "add"))
			{
				stage->blendSrc = BGFX_STATE_BLEND_ONE;
				stage->blendDst = BGFX_STATE_BLEND_ONE;
			} 
			else if (!Q_stricmp(token, "filter")) 
			{
				stage->blendSrc = BGFX_STATE_BLEND_DST_COLOR;
				stage->blendDst = BGFX_STATE_BLEND_ZERO;
			} 
			else if (!Q_stricmp(token, "blend")) 
			{
				stage->blendSrc = BGFX_STATE_BLEND_SRC_ALPHA;
				stage->blendDst = BGFX_STATE_BLEND_INV_SRC_ALPHA;
			} 
			else 
			{
				// complex double blends
				stage->blendSrc = srcBlendModeFromName(token);

				token = COM_ParseExt(text, qfalse);

				if (token[0] == 0)
				{
					ri.Printf(PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", name);
					continue;
				}

				stage->blendDst = dstBlendModeFromName(token);
			}

			// clear depth write for blended surfaces
			if (!depthWriteExplicit)
			{
				stage->depthWrite = false;
			}
		}
		// stage <type>
		else if (!Q_stricmp(token, "stage"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameters for stage in shader '%s'\n", name);
			}
			else if (!Q_stricmp(token, "diffuseMap"))
			{
				stage->type = MaterialStageType::DiffuseMap;
			}
			else if (!Q_stricmp(token, "normalMap") || !Q_stricmp(token, "bumpMap"))
			{
				stage->type = MaterialStageType::NormalMap;
				//VectorSet4(stage->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
			}
			else if (!Q_stricmp(token, "normalParallaxMap") || !Q_stricmp(token, "bumpParallaxMap"))
			{
				/*if (r_parallaxMapping->integer)
					stage->type = MaterialStageType::NormalParallaxMap;
				else
					stage->type = MaterialStageType::NormalMap;

				VectorSet4(stage->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);*/
			}
			else if (!Q_stricmp(token, "specularMap"))
			{
				stage->type = MaterialStageType::SpecularMap;
				stage->specularScale = { 1, 1, 1, 1 };
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown stage parameter '%s' in shader '%s'\n", token, name);
			}
		}
		// specularReflectance <value>
		else if (!Q_stricmp(token, "specularreflectance"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for specular reflectance in shader '%s'\n", name);
				continue;
			}

			stage->specularScale = vec4(atof(token));
		}
		// specularExponent <value>
		else if (!Q_stricmp(token, "specularexponent"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for specular exponent in shader '%s'\n", name);
				continue;
			}

			// Change shininess to gloss
			// FIXME: assumes max exponent of 8192 and min of 1, must change here if altered in lightall_fp.glsl
			const float exponent = math::Clamped((float)atof(token), 1.0f, 8192.0f);
			stage->specularScale.a = log(exponent) / log(8192.0f);
		}
		// gloss <value>
		else if (!Q_stricmp(token, "gloss"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for gloss in shader '%s'\n", name);
				continue;
			}

			stage->specularScale.a = atof(token);
		}
		// parallaxDepth <value>
		else if (!Q_stricmp(token, "parallaxdepth"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for parallaxDepth in shader '%s'\n", name);
				continue;
			}

			stage->normalScale.w = atof(token);
		}
		// normalScale <xy>
		// or normalScale <x> <y>
		// or normalScale <x> <y> <height>
		else if (!Q_stricmp(token, "normalscale"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for normalScale in shader '%s'\n", name);
				continue;
			}

			stage->normalScale.x = atof(token);
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				// one value, applies to X/Y
				stage->normalScale.y = stage->normalScale.x;
				continue;
			}

			stage->normalScale.y = atof(token);
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
				continue; // two values, no height

			stage->normalScale.z = atof(token);
		}
		// specularScale <rgb> <gloss>
		// or specularScale <r> <g> <b>
		// or specularScale <r> <g> <b> <gloss>
		else if (!Q_stricmp(token, "specularscale"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for specularScale in shader '%s'\n", name);
				continue;
			}

			stage->specularScale.r = atof(token);
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for specularScale in shader '%s'\n", name);
				continue;
			}

			stage->specularScale.g = atof(token);
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				// two values, rgb then gloss
				stage->specularScale.a = stage->specularScale.g;
				stage->specularScale.g = stage->specularScale.r;
				stage->specularScale.b = stage->specularScale.r;
				continue;
			}

			stage->specularScale.b = atof(token);
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				// three values, rgb
				continue;
			}

			stage->specularScale.b = atof(token);

		}
		// rgbGen
		else if (!Q_stricmp(token, "rgbGen"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", name);
			}
			else if (!Q_stricmp(token, "wave"))
			{
				stage->rgbWave = parseWaveForm(text);
				stage->rgbGen = MaterialColorGen::Waveform;
			}
			else if (!Q_stricmp(token, "const"))
			{
				auto color = parseVector(text);
				stage->constantColor[0] = 255 * color.r;
				stage->constantColor[1] = 255 * color.g;
				stage->constantColor[2] = 255 * color.b;
				stage->rgbGen = MaterialColorGen::Const;
			}
			else if (!Q_stricmp(token, "identity"))
			{
				stage->rgbGen = MaterialColorGen::Identity;
			}
			else if (!Q_stricmp(token, "identityLighting"))
			{
				stage->rgbGen = MaterialColorGen::IdentityLighting;
			}
			else if (!Q_stricmp(token, "entity"))
			{
				stage->rgbGen = MaterialColorGen::Entity;
			}
			else if (!Q_stricmp(token, "oneMinusEntity"))
			{
				stage->rgbGen = MaterialColorGen::OneMinusEntity;
			}
			else if (!Q_stricmp(token, "vertex"))
			{
				stage->rgbGen = MaterialColorGen::Vertex;

				if (stage->alphaGen == MaterialAlphaGen::Identity)
					stage->alphaGen = MaterialAlphaGen::Vertex;
			}
			else if (!Q_stricmp(token, "exactVertex"))
			{
				stage->rgbGen = MaterialColorGen::ExactVertex;
			}
			else if (!Q_stricmp(token, "vertexLit"))
			{
				stage->rgbGen = MaterialColorGen::VertexLit;

				if (stage->alphaGen == MaterialAlphaGen::Identity)
					stage->alphaGen = MaterialAlphaGen::Vertex;
			}
			else if (!Q_stricmp(token, "exactVertexLit"))
			{
				stage->rgbGen = MaterialColorGen::ExactVertexLit;
			}
			else if (!Q_stricmp(token, "lightingDiffuse"))
			{
				stage->rgbGen = MaterialColorGen::LightingDiffuse;
			}
			else if (!Q_stricmp(token, "oneMinusVertex"))
			{
				stage->rgbGen = MaterialColorGen::OneMinusVertex;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, name);
			}
		}
		// alphaGen 
		else if (!Q_stricmp(token, "alphaGen"))
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", name);
			}
			else if (!Q_stricmp(token, "wave"))
			{
				stage->alphaWave = parseWaveForm(text);
				stage->alphaGen = MaterialAlphaGen::Waveform;
			}
			else if (!Q_stricmp(token, "const"))
			{
				token = COM_ParseExt(text, qfalse);
				stage->constantColor[3] = 255 * atof(token);
				stage->alphaGen = MaterialAlphaGen::Const;
			}
			else if (!Q_stricmp(token, "identity"))
			{
				stage->alphaGen = MaterialAlphaGen::Identity;
			}
			else if (!Q_stricmp(token, "entity"))
			{
				stage->alphaGen = MaterialAlphaGen::Entity;
			}
			else if (!Q_stricmp(token, "oneMinusEntity"))
			{
				stage->alphaGen = MaterialAlphaGen::OneMinusEntity;
			}
			else if (!Q_stricmp(token, "vertex"))
			{
				stage->alphaGen = MaterialAlphaGen::Vertex;
			}
			else if (!Q_stricmp(token, "lightingSpecular"))
			{
				stage->alphaGen = MaterialAlphaGen::LightingSpecular;
			}
			else if (!Q_stricmp(token, "oneMinusVertex"))
			{
				stage->alphaGen = MaterialAlphaGen::OneMinusVertex;
			}
			else if (!Q_stricmp(token, "portal"))
			{
				stage->alphaGen = MaterialAlphaGen::Portal;
				token = COM_ParseExt(text, qfalse);

				if (token[0] == 0)
				{
					ri.Printf(PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to %i\n", name, portalRange);
				}
				else
				{
					portalRange = atof(token);
				}
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, name);
				continue;
			}
		}
		// tcGen <function>
		else if (!Q_stricmp(token, "texgen") || !Q_stricmp(token, "tcGen")) 
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", name);
			}
			else if (!Q_stricmp(token, "environment"))
			{
				stage->bundles[0].tcGen = MaterialTexCoordGen::EnvironmentMapped;
			}
			else if (!Q_stricmp(token, "lightmap"))
			{
				stage->bundles[0].tcGen = MaterialTexCoordGen::Lightmap;
			}
			else if (!Q_stricmp(token, "texture") || !Q_stricmp(token, "base"))
			{
				stage->bundles[0].tcGen = MaterialTexCoordGen::Texture;
			}
			else if (!Q_stricmp(token, "vector"))
			{
				stage->bundles[0].tcGenVectors[0] = parseVector(text);
				stage->bundles[0].tcGenVectors[1] = parseVector(text);
				stage->bundles[0].tcGen = MaterialTexCoordGen::Vector;
			}
			else 
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", name);
			}
		}
		// tcMod <type> <...>
		else if (!Q_stricmp(token, "tcMod"))
		{
			if (stage->bundles[0].numTexMods == MaterialTextureBundle::maxTexMods)
			{
				ri.Printf(PRINT_WARNING, "WARNING: too many tcMod stages in shader '%s'", name);
				continue;
			}

			char buffer[1024] = "";

			while (1)
			{
				token = COM_ParseExt(text, qfalse);

				if (token[0] == 0)
					break;

				Q_strcat(buffer, sizeof (buffer), token);
				Q_strcat(buffer, sizeof (buffer), " ");
			}

			stage->bundles[0].texMods[stage->bundles[0].numTexMods] = parseTexMod(buffer);
			stage->bundles[0].numTexMods++;
		}
		// depthmask
		else if (!Q_stricmp(token, "depthwrite"))
		{
			stage->depthWrite = true;
			depthWriteExplicit = true;
		}
		else
		{
			ri.Printf(PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, name);
			return false;
		}
	}

	// if cgen isn't explicitly specified, use either identity or identitylighting
	if (stage->rgbGen == MaterialColorGen::Bad)
	{
		if (stage->blendSrc == 0 || stage->blendSrc == BGFX_STATE_BLEND_ONE ||  stage->blendSrc == BGFX_STATE_BLEND_SRC_ALPHA)
		{
			stage->rgbGen = MaterialColorGen::IdentityLighting;
		}
		else
		{
			stage->rgbGen = MaterialColorGen::Identity;
		}
	}

	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	if (stage->blendSrc == BGFX_STATE_BLEND_ONE && stage->blendDst == BGFX_STATE_BLEND_ZERO)
	{
		stage->blendSrc = stage->blendDst = 0;
		stage->depthWrite = true;
	}

	// decide which agens we can skip
	if (stage->alphaGen == MaterialAlphaGen::Identity)
	{
		if (stage->rgbGen == MaterialColorGen::Identity || stage->rgbGen == MaterialColorGen::LightingDiffuse)
		{
			stage->alphaGen = MaterialAlphaGen::Skip;
		}
	}

	return true;
}

MaterialWaveForm Material::parseWaveForm(char **text) const
{
	MaterialWaveForm wave;
	auto token = COM_ParseExt(text, qfalse);

	if (token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.func = genFuncFromName(token);

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt(text, qfalse);

	if (token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.base = atof(token);
	token = COM_ParseExt(text, qfalse);

	if (token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.amplitude = atof(token);
	token = COM_ParseExt(text, qfalse);

	if (token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.phase = atof(token);
	token = COM_ParseExt(text, qfalse);

	if (token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.frequency = atof(token);
	return wave;
}

MaterialTexModInfo Material::parseTexMod(char *buffer) const
{
	MaterialTexModInfo tmi;
	char **text = &buffer;
	auto token = COM_ParseExt(text, qfalse);

	if (!Q_stricmp(token, "turb"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.base = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.amplitude = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.phase = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.frequency = atof(token);
		tmi.type = MaterialTexMod::Turbulent;
	}
	else if (!Q_stricmp(token, "scale"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.scale[0] = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.scale[1] = atof(token);
		tmi.type = MaterialTexMod::Scale;
	}
	else if (!Q_stricmp(token, "scroll"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.scroll[0] = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.scroll[1] = atof(token);
		tmi.type = MaterialTexMod::Scroll;
	}
	else if (!Q_stricmp(token, "stretch"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.func = genFuncFromName(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.base = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.amplitude = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.phase = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.frequency = atof(token);
		tmi.type = MaterialTexMod::Stretch;
	}
	else if (!Q_stricmp(token, "transform"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.matrix[0][0] = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.matrix[0][1] = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.matrix[1][0] = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.matrix[1][1] = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.translate[0] = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.translate[1] = atof(token);
		tmi.type = MaterialTexMod::Transform;
	}
	else if (!Q_stricmp(token, "rotate"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.rotateSpeed = atof(token);
		tmi.type = MaterialTexMod::Rotate;
	}
	else if (!Q_stricmp(token, "entityTranslate"))
	{
		tmi.type = MaterialTexMod::EntityTranslate;
	}
	else
	{
		ri.Printf(PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, name);
	}

	return tmi;
}

/*
deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]
*/
MaterialDeformStage Material::parseDeform(char **text) const
{
	MaterialDeformStage ds;
	auto token = COM_ParseExt(text, qfalse);

	if (token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", name);
	}
	else if (!Q_stricmp(token, "projectionShadow"))
	{
		ds.deformation = MaterialDeform::ProjectionShadow;
	}
	else if (!Q_stricmp(token, "autosprite"))
	{
		ds.deformation = MaterialDeform::Autosprite;
	}
	else if (!Q_stricmp(token, "autosprite2"))
	{
		ds.deformation = MaterialDeform::Autosprite2;
	}
	else if (!Q_stricmpn(token, "text", 4))
	{
		int n = token[4] - '0';

		if (n < 0 || n > 7)
			n = 0;

		ds.deformation = MaterialDeform((int)MaterialDeform::Text0 + n);
	}
	else if (!Q_stricmp(token, "bulge"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", name);
			return ds;
		}

		ds.bulgeWidth = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", name);
			return ds;
		}

		ds.bulgeHeight = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", name);
			return ds;
		}

		ds.bulgeSpeed = atof(token);
		ds.deformation = MaterialDeform::Bulge;
	}
	else if (!Q_stricmp(token, "wave"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", name);
			return ds;
		}

		if (atof(token) != 0)
		{
			ds.deformationSpread = 1.0f / atof(token);
		}
		else
		{
			ds.deformationSpread = 100.0f;
			ri.Printf(PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", name);
		}

		ds.deformationWave = parseWaveForm(text);
		ds.deformation = MaterialDeform::Wave;
	}
	else if (!Q_stricmp(token, "normal"))
	{
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", name);
			return ds;
		}

		ds.deformationWave.amplitude = atof(token);
		token = COM_ParseExt(text, qfalse);

		if (token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", name);
			return ds;
		}

		ds.deformationWave.frequency = atof(token);
		ds.deformation = MaterialDeform::Normals;
	}
	else if (!Q_stricmp(token, "move"))
	{
		for (size_t i = 0; i < 3; i++)
		{
			token = COM_ParseExt(text, qfalse);

			if (token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", name);
				return ds;
			}

			ds.moveVector[i] = atof(token);
		}

		ds.deformationWave = parseWaveForm(text);
		ds.deformation = MaterialDeform::Move;
	}
	else
	{
		ri.Printf(PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, name);
	}

	return ds;
}

// skyParms <outerbox> <cloudheight> <innerbox>
void Material::parseSkyParms(char **text)
{
	const char * const suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	const int imgFlags = TextureFlags::Mipmap | TextureFlags::Picmip;

	// outerbox
	auto token = COM_ParseExt(text, qfalse);

	if (token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", name);
		return;
	}

	if (strcmp(token, "-"))
	{
		for (size_t i = 0; i < 6; i++)
		{
			char pathname[MAX_QPATH];
			Com_sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
			sky.outerbox[i] = g_main->textureCache->findTexture(pathname, TextureType::ColorAlpha, imgFlags | TextureFlags::ClampToEdge);

			if (!sky.outerbox[i])
			{
				sky.outerbox[i] = g_main->textureCache->getDefaultTexture();
			}
		}
	}

	// cloudheight
	token = COM_ParseExt(text, qfalse);

	if (token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", name);
		return;
	}

	sky.cloudHeight = atof(token);

	if (!sky.cloudHeight)
	{
		sky.cloudHeight = 512;
	}

	Sky_InitializeTexCoords(sky.cloudHeight);

	// innerbox
	token = COM_ParseExt(text, qfalse);

	if (token[0] == 0) 
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", name);
		return;
	}

	if (strcmp(token, "-"))
	{
		for (size_t i=0 ; i<6 ; i++) 
		{
			char pathname[MAX_QPATH];
			Com_sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
			sky.innerbox[i] = g_main->textureCache->findTexture(pathname, TextureType::ColorAlpha, imgFlags);

			if (!sky.innerbox[i])
			{
				sky.innerbox[i] = g_main->textureCache->getDefaultTexture();
			}
		}
	}

	isSky = true;
}

MaterialAlphaTest Material::alphaTestFromName(const char *name) const
{
	MaterialAlphaTest value = MaterialAlphaTest::None;
	NameToValueConverter<MaterialAlphaTest> converter;

	if (!converter.convert(name, &value, 
	{
		{ "GT0", MaterialAlphaTest::GT_0 },
		{ "LT128", MaterialAlphaTest::LT_128 },
		{ "GE128", MaterialAlphaTest::GE_128 }
	}))
	{
		ri.Printf(PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", name, this->name);
	}
	
	return value;
}

uint64_t Material::srcBlendModeFromName(const char *name) const
{
	uint64_t value = BGFX_STATE_BLEND_ONE;
	NameToValueConverter<uint64_t> converter;

	if (!converter.convert(name, &value, 
	{
		{ "GL_ONE", BGFX_STATE_BLEND_ONE },
		{ "GL_ZERO", BGFX_STATE_BLEND_ZERO },
		{ "GL_DST_COLOR", BGFX_STATE_BLEND_DST_COLOR },
		{ "GL_ONE_MINUS_DST_COLOR", BGFX_STATE_BLEND_INV_DST_COLOR },
		{ "GL_SRC_ALPHA", BGFX_STATE_BLEND_SRC_ALPHA },
		{ "GL_ONE_MINUS_SRC_ALPHA", BGFX_STATE_BLEND_INV_SRC_ALPHA },
		{ "GL_DST_ALPHA", BGFX_STATE_BLEND_DST_ALPHA },
		{ "GL_ONE_MINUS_DST_ALPHA", BGFX_STATE_BLEND_INV_DST_ALPHA },
		{ "GL_SRC_ALPHA_SATURATE", BGFX_STATE_BLEND_SRC_ALPHA_SAT }
	}))
	{
		ri.Printf(PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, this->name);
	}
	
	return value;
}

uint64_t Material::dstBlendModeFromName(const char *name) const
{
	uint64_t value = BGFX_STATE_BLEND_ONE;
	NameToValueConverter<uint64_t> converter;

	if (!converter.convert(name, &value, 
	{
		{ "GL_ONE", BGFX_STATE_BLEND_ONE },
		{ "GL_ZERO", BGFX_STATE_BLEND_ZERO },
		{ "GL_SRC_ALPHA", BGFX_STATE_BLEND_SRC_ALPHA },
		{ "GL_ONE_MINUS_SRC_ALPHA", BGFX_STATE_BLEND_INV_SRC_ALPHA },
		{ "GL_DST_ALPHA", BGFX_STATE_BLEND_DST_ALPHA },
		{ "GL_ONE_MINUS_DST_ALPHA", BGFX_STATE_BLEND_INV_DST_ALPHA },
		{ "GL_SRC_COLOR", BGFX_STATE_BLEND_SRC_COLOR },
		{ "GL_ONE_MINUS_SRC_COLOR", BGFX_STATE_BLEND_INV_SRC_COLOR }
	}))
	{
		ri.Printf(PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, this->name);
	}

	return value;
}

MaterialWaveformGenFunc Material::genFuncFromName(const char *name) const
{
	MaterialWaveformGenFunc value = MaterialWaveformGenFunc::Sin;
	NameToValueConverter<MaterialWaveformGenFunc> converter;

	if (!converter.convert(name, &value, 
	{
		{ "sin", MaterialWaveformGenFunc::Sin },
		{ "square", MaterialWaveformGenFunc::Square },
		{ "triangle", MaterialWaveformGenFunc::Triangle },
		{ "sawtooth", MaterialWaveformGenFunc::Sawtooth },
		{ "inversesawtooth", MaterialWaveformGenFunc::InverseSawtooth },
		{ "noise", MaterialWaveformGenFunc::Noise }
	}))
	{
		ri.Printf(PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", name, this->name);
	}

	return value;
}

float Material::sortFromName(const char *name) const
{
	float value = 0;
	NameToValueConverter<float> converter;

	if (!converter.convert(name, &value, 
	{
		{ "portal", MaterialSort::Portal },
		{ "sky", MaterialSort::Environment },
		{ "opaque", MaterialSort::Opaque },
		{ "decal", MaterialSort::Decal },
		{ "seeThrough", MaterialSort::SeeThrough },
		{ "banner", MaterialSort::Banner },
		{ "additive", MaterialSort::Blend1 },
		{ "nearest", MaterialSort::Nearest },
		{ "underwater", MaterialSort::Underwater }
	}))
	{
		value = atof(name);
	}

	return value;
}

} // namespace renderer
