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
		for (const Pair &p : pairs)
		{
			if (util::Stricmp(p.name, name) == 0)
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
	char *token = util::Parse(text, true);

	if (token[0] != '{')
	{
		interface::PrintWarningf("WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, name);
		return false;
	}

	int stageIndex = 0;

	for (;;)
	{
		token = util::Parse(text, true);

		if (!token[0])
		{
			interface::PrintWarningf("WARNING: no concluding '}' in shader %s\n", name);
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
				interface::PrintWarningf("WARNING: too many stages in shader %s (max is %i)\n", name, (int)maxStages);
				return false;
			}

			if (!parseStage(&stages[stageIndex], text))
				return false;

			stages[stageIndex].active = true;
			stageIndex++;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if (!util::Stricmpn(token, "qer", 3))
		{
			util::SkipRestOfLine(text);
		}
		// sun parms
		else if (!util::Stricmp(token, "q3map_sun") || !util::Stricmp(token, "q3map_sunExt") || !util::Stricmp(token, "q3gl2_sun"))
		{
			SunLight sun;

			if (!util::Stricmp(token, "q3gl2_sun"))
			{
				sun.shadows = true;
			}

			token = util::Parse(text, false);
			sun.light[0] = (float)atof(token);
			token = util::Parse(text, false);
			sun.light[1] = (float)atof(token);
			token = util::Parse(text, false);
			sun.light[2] = (float)atof(token);
			
			token = util::Parse(text, false);
			sun.light.normalize();
			sun.light = sun.light * (float)atof(token) * (float)g_overbrightFactor / 255.0f;

			token = util::Parse(text, false);
			float a = (float)atof(token) / 180 * (float)M_PI;

			token = util::Parse(text, false);
			float b = (float)atof(token) / 180 * (float)M_PI;

			sun.direction[0] = cos(a) * cos(b);
			sun.direction[1] = sin(a) * cos(b);
			sun.direction[2] = sin(b);
			sun.direction.normalize();

			if (sun.shadows)
			{
				token = util::Parse(text, false);
				sun.lightScale = (float)atof(token);

				token = util::Parse(text, false);
				sun.shadowScale = (float)atof(token);
			}

			main::SetSunLight(sun);
			util::SkipRestOfLine(text);
		}
		// tonemap parms
		else if (!util::Stricmp(token, "q3gl2_tonemap"))
		{
			vec2 autoExposureMinMax = { -2, 2 };
			vec3 toneMinAvgMaxLevel = { -8, -2, 0 };

			token = util::Parse(text, false);
			toneMinAvgMaxLevel[0] = (float)atof(token);
			token = util::Parse(text, false);
			toneMinAvgMaxLevel[1] = (float)atof(token);
			token = util::Parse(text, false);
			toneMinAvgMaxLevel[2] = (float)atof(token);

			token = util::Parse(text, false);
			autoExposureMinMax[0] = (float)atof(token);
			token = util::Parse(text, false);
			autoExposureMinMax[1] = (float)atof(token);

			util::SkipRestOfLine(text);
		}
		else if (!util::Stricmp(token, "deformVertexes"))
		{
			if (numDeforms == maxDeforms)
			{
				interface::PrintWarningf("WARNING: max deforms in '%s'\n", name);
				continue;
			}

			deforms[numDeforms] = parseDeform(text);
			numDeforms++;
		}
		else if (!util::Stricmp(token, "tesssize"))
		{
			util::SkipRestOfLine(text);
		}
		else if (!util::Stricmp(token, "clampTime")) 
		{
			token = util::Parse(text, false);

			if (token[0])
			{
				clampTime = (float)atof(token);
			}
		}
		// skip stuff that only the q3map needs
		else if (!util::Stricmpn(token, "q3map", 5))
		{
			util::SkipRestOfLine(text);
		}
		// skip stuff that only q3map or the server needs
		else if (!util::Stricmp(token, "surfaceParm"))
		{
			const size_t nInfoParms = BX_COUNTOF(infoParms);
			token = util::Parse(text, false);

			for (size_t i = 0; i < nInfoParms; i++)
			{
				if (!util::Stricmp(token, infoParms[i].name))
				{
					surfaceFlags |= infoParms[i].surfaceFlags;
					contentFlags |= infoParms[i].contents;
					break;
				}
			}
		}
		// no mip maps
		else if (!util::Stricmp(token, "nomipmaps"))
		{
			noMipMaps = true;
			noPicMip = true;
		}
		// no picmip adjustment
		else if (!util::Stricmp(token, "nopicmip"))
		{
			noPicMip = true;
		}
		// polygonOffset
		else if (!util::Stricmp(token, "polygonOffset"))
		{
			polygonOffset = true;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if (!util::Stricmp(token, "entityMergable"))
		{
			entityMergable = true;
		}
		// fogParms
		else if (!util::Stricmp(token, "fogParms"))
		{
			bool parsedColor;
			fogParms.color = parseVector(text, &parsedColor);

			if (!parsedColor)
			{
				return false;
			}

			token = util::Parse(text, false);

			if (!token[0]) 
			{
				interface::PrintWarningf("WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", name);
				continue;
			}

			fogParms.depthForOpaque = (float)atof(token);

			// skip any old gradient directions
			util::SkipRestOfLine(text);
		}
		// portal
		else if (!util::Stricmp(token, "portal"))
		{
			sort = MaterialSort::Portal;
			isPortal = true;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if (!util::Stricmp(token, "skyparms"))
		{
			parseSkyParms(text);
		}
		// light <value> determines flaring in q3map, not needed here
		else if (!util::Stricmp(token, "light")) 
		{
			util::Parse(text, false);
		}
		// cull <face>
		else if (!util::Stricmp(token, "cull")) 
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing cull parms in shader '%s'\n", name);
			}
			else if (!util::Stricmp(token, "none") || !util::Stricmp(token, "twosided") || !util::Stricmp(token, "disable"))
			{
				cullType = MaterialCullType::TwoSided;
			}
			else if (!util::Stricmp(token, "back") || !util::Stricmp(token, "backside") || !util::Stricmp(token, "backsided"))
			{
				cullType = MaterialCullType::BackSided;
			}
			else
			{
				interface::PrintWarningf("WARNING: invalid cull parm '%s' in shader '%s'\n", token, name);
			}
		}
		// sort
		else if (!util::Stricmp(token, "sort"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing sort parameter in shader '%s'\n", name);
				continue;
			}

			sort = sortFromName(token);
		}
		else
		{
			interface::PrintWarningf("WARNING: unknown general shader parameter '%s' in '%s'\n", token, name);
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
	char *token = util::Parse(text, false);

	if (strcmp(token, "("))
	{
		interface::PrintWarningf("WARNING: missing parenthesis in shader '%s'\n", name);

		if (result)
			*result = false;

		return v;
	}

	for (size_t i = 0; i < 3; i++)
	{
		token = util::Parse(text, false);

		if (!token[0])
		{
			interface::PrintWarningf("WARNING: missing vector element in shader '%s'\n", name);

			if (result)
				*result = false;

			return v;
		}

		v[i] = (float)atof(token);
	}

	token = util::Parse(text, false);

	if (strcmp(token, ")"))
	{
		interface::PrintWarningf("WARNING: missing parenthesis in shader '%s'\n", name);

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
		char *token = util::Parse(text, true);

		if (!token[0])
		{
			interface::PrintWarningf("WARNING: no matching '}' found\n");
			return false;
		}

		if (token[0] == '}')
			break;
		
		// map <name>
		if (!util::Stricmp(token, "map"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("WARNING: missing parameter for 'map' keyword in shader '%s'\n", name);
				return false;
			}

			if (!util::Stricmp(token, "$whiteimage"))
			{
				stage->bundles[0].textures[0] = Texture::getWhite();
			}
			else if (!util::Stricmp(token, "$lightmap"))
			{
				stage->bundles[0].isLightmap = true;
				const Texture *lightmap = world::IsLoaded() ? world::GetLightmap(lightmapIndex) : nullptr;

				if (!lightmap)
				{
					lightmap = Texture::getWhite();
				}
				
				stage->bundles[0].textures[0] = lightmap;
			}
			else if (!util::Stricmp(token, "$deluxemap"))
			{
				/*if (!tr.worldDeluxeMapping)
				{
					interface::PrintWarningf("WARNING: shader '%s' wants a deluxe map in a map compiled without them\n", name);
					return qfalse;
				}*/

				stage->bundles[0].isLightmap = true;
				stage->bundles[0].textures[0] = Texture::getWhite();

				/*if (lightmapIndex < 0)
				{
					stage->bundles[0].textures[0] = Texture::getWhite();
				}
				else
				{
					stage->bundles[0].textures[0] = tr.deluxemaps[lightmapIndex];
				}*/
			}
			else
			{
				int flags = TextureFlags::None;

				if (!noMipMaps)
					flags |= TextureFlags::Mipmap;

				if (!noPicMip)
					flags |= TextureFlags::Picmip;

				stage->bundles[0].textures[0] = Texture::find(token, flags);

				if (!stage->bundles[0].textures[0])
				{
					interface::PrintWarningf("WARNING: could not find texture '%s' in shader '%s'\n", token, name);
					return false;
				}
			}
		}
		// clampmap <name>
		else if (!util::Stricmp(token, "clampmap"))
		{
			int flags = TextureFlags::ClampToEdge;

			token = util::Parse(text, false);
			if (!token[0])
			{
				interface::PrintWarningf("WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", name);
				return false;
			}

			if (!noMipMaps)
				flags |= TextureFlags::Mipmap;

			if (!noPicMip)
				flags |= TextureFlags::Picmip;

			stage->bundles[0].textures[0] = Texture::find(token, flags);

			if (!stage->bundles[0].textures[0])
			{
				interface::PrintWarningf("WARNING: could not find texture '%s' in shader '%s'\n", token, name);
				return false;
			}
		}
		// animMap <frequency> <image1> .... <imageN>
		else if (!util::Stricmp(token, "animMap"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("WARNING: missing parameter for 'animMap' keyword in shader '%s'\n", name);
				return false;
			}

			stage->bundles[0].imageAnimationSpeed = (float)atof(token);

			// parse up to MaterialTextureBundle::maxImageAnimations animations
			for (;;)
			{
				token = util::Parse(text, false);

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

					stage->bundles[0].textures[num] = Texture::find(token, flags);

					if (!stage->bundles[0].textures[num])
					{
						interface::PrintWarningf("WARNING: could not find texture '%s' in shader '%s'\n", token, name);
						return false;
					}

					stage->bundles[0].numImageAnimations++;
				}
			}
		}
		else if (!util::Stricmp(token, "videoMap"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", name);
				return false;
			}

			stage->bundles[0].videoMapHandle = interface::CIN_PlayCinematic(token, 0, 0, 256, 256);

			if (stage->bundles[0].videoMapHandle != -1)
			{
				stage->bundles[0].isVideoMap = true;
				stage->bundles[0].textures[0] = Texture::getScratch(size_t(stage->bundles[0].videoMapHandle));
			}
		}
		// alphafunc <func>
		else if (!util::Stricmp(token, "alphaFunc"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", name);
				return false;
			}


			stage->alphaTest = alphaTestFromName(token);
		}
		// depthFunc <func>
		else if (!util::Stricmp(token, "depthfunc"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", name);
				return false;
			}

			if (!util::Stricmp(token, "lequal"))
			{
				stage->depthTestBits = BGFX_STATE_DEPTH_TEST_LEQUAL;
			}
			else if (!util::Stricmp(token, "equal"))
			{
				stage->depthTestBits = BGFX_STATE_DEPTH_TEST_EQUAL;
			}
			else
			{
				interface::PrintWarningf("WARNING: unknown depthfunc '%s' in shader '%s'\n", token, name);
			}
		}
		// detail
		else if (!util::Stricmp(token, "detail"))
		{
			stage->isDetail = true;
		}
		// blendfunc <srcFactor> <dstFactor> or blendfunc <add|filter|blend>
		else if (!util::Stricmp(token, "blendfunc"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parm for blendFunc in shader '%s'\n", name);
				continue;
			}

			// check for "simple" blends first
			if (!util::Stricmp(token, "add"))
			{
				stage->blendSrc = BGFX_STATE_BLEND_ONE;
				stage->blendDst = BGFX_STATE_BLEND_ONE;
			}
			else if (!util::Stricmp(token, "filter"))
			{
				stage->blendSrc = BGFX_STATE_BLEND_DST_COLOR;
				stage->blendDst = BGFX_STATE_BLEND_ZERO;
			}
			else if (!util::Stricmp(token, "blend"))
			{
				stage->blendSrc = BGFX_STATE_BLEND_SRC_ALPHA;
				stage->blendDst = BGFX_STATE_BLEND_INV_SRC_ALPHA;
			}
			else
			{
				// complex double blends
				stage->blendSrc = srcBlendModeFromName(token);

				token = util::Parse(text, false);

				if (token[0] == 0)
				{
					interface::PrintWarningf("WARNING: missing parm for blendFunc in shader '%s'\n", name);
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
		else if (!util::Stricmp(token, "stage"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameters for stage in shader '%s'\n", name);
			}
			else if (!util::Stricmp(token, "diffuseMap"))
			{
				stage->type = MaterialStageType::DiffuseMap;
			}
			else if (!util::Stricmp(token, "normalMap") || !util::Stricmp(token, "bumpMap"))
			{
				stage->type = MaterialStageType::NormalMap;
				//VectorSet4(stage->normalScale, r_baseNormalX.getFloat(), r_baseNormalY.getFloat(), 1.0f, r_baseParallax.getFloat());
			}
			else if (!util::Stricmp(token, "normalParallaxMap") || !util::Stricmp(token, "bumpParallaxMap"))
			{
				/*if (r_parallaxMapping->integer)
					stage->type = MaterialStageType::NormalParallaxMap;
				else
					stage->type = MaterialStageType::NormalMap;

				VectorSet4(stage->normalScale, r_baseNormalX.getFloat(), r_baseNormalY.getFloat(), 1.0f, r_baseParallax.getFloat());*/
			}
			else if (!util::Stricmp(token, "specularMap"))
			{
				stage->type = MaterialStageType::SpecularMap;
				stage->specularScale = { 1, 1, 1, 1 };
			}
			else
			{
				interface::PrintWarningf("WARNING: unknown stage parameter '%s' in shader '%s'\n", token, name);
			}
		}
		// specularReflectance <value>
		else if (!util::Stricmp(token, "specularreflectance"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameter for specular reflectance in shader '%s'\n", name);
				continue;
			}

			stage->specularScale = vec4((float)atof(token));
		}
		// specularExponent <value>
		else if (!util::Stricmp(token, "specularexponent"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameter for specular exponent in shader '%s'\n", name);
				continue;
			}

			// Change shininess to gloss
			// FIXME: assumes max exponent of 8192 and min of 1, must change here if altered in lightall_fp.glsl
			const float exponent = math::Clamped((float)atof(token), 1.0f, 8192.0f);
			stage->specularScale.a = log(exponent) / log(8192.0f);
		}
		// gloss <value>
		else if (!util::Stricmp(token, "gloss"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameter for gloss in shader '%s'\n", name);
				continue;
			}

			stage->specularScale.a = (float)atof(token);
		}
		// parallaxDepth <value>
		else if (!util::Stricmp(token, "parallaxdepth"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameter for parallaxDepth in shader '%s'\n", name);
				continue;
			}

			stage->normalScale.w = (float)atof(token);
		}
		// normalScale <xy>
		// or normalScale <x> <y>
		// or normalScale <x> <y> <height>
		else if (!util::Stricmp(token, "normalscale"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameter for normalScale in shader '%s'\n", name);
				continue;
			}

			stage->normalScale.x = (float)atof(token);
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				// one value, applies to X/Y
				stage->normalScale.y = stage->normalScale.x;
				continue;
			}

			stage->normalScale.y = (float)atof(token);
			token = util::Parse(text, false);

			if (token[0] == 0)
				continue; // two values, no height

			stage->normalScale.z = (float)atof(token);
		}
		// specularScale <rgb> <gloss>
		// or specularScale <r> <g> <b>
		// or specularScale <r> <g> <b> <gloss>
		else if (!util::Stricmp(token, "specularscale"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameter for specularScale in shader '%s'\n", name);
				continue;
			}

			stage->specularScale.r = (float)atof(token);
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameter for specularScale in shader '%s'\n", name);
				continue;
			}

			stage->specularScale.g = (float)atof(token);
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				// two values, rgb then gloss
				stage->specularScale.a = stage->specularScale.g;
				stage->specularScale.g = stage->specularScale.r;
				stage->specularScale.b = stage->specularScale.r;
				continue;
			}

			stage->specularScale.b = (float)atof(token);
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				// three values, rgb
				continue;
			}

			stage->specularScale.b = (float)atof(token);

		}
		// rgbGen
		else if (!util::Stricmp(token, "rgbGen"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameters for rgbGen in shader '%s'\n", name);
			}
			else if (!util::Stricmp(token, "wave"))
			{
				stage->rgbWave = parseWaveForm(text);
				stage->rgbGen = MaterialColorGen::Waveform;
			}
			else if (!util::Stricmp(token, "const"))
			{
				stage->constantColor = vec4(parseVector(text), stage->constantColor.a);
				stage->rgbGen = MaterialColorGen::Const;
			}
			else if (!util::Stricmp(token, "identity"))
			{
				stage->rgbGen = MaterialColorGen::Identity;
			}
			else if (!util::Stricmp(token, "identityLighting"))
			{
				stage->rgbGen = MaterialColorGen::IdentityLighting;
			}
			else if (!util::Stricmp(token, "entity"))
			{
				stage->rgbGen = MaterialColorGen::Entity;
			}
			else if (!util::Stricmp(token, "oneMinusEntity"))
			{
				stage->rgbGen = MaterialColorGen::OneMinusEntity;
			}
			else if (!util::Stricmp(token, "vertex"))
			{
				stage->rgbGen = MaterialColorGen::Vertex;

				if (stage->alphaGen == MaterialAlphaGen::Identity)
					stage->alphaGen = MaterialAlphaGen::Vertex;
			}
			else if (!util::Stricmp(token, "exactVertex"))
			{
				stage->rgbGen = MaterialColorGen::ExactVertex;
			}
			else if (!util::Stricmp(token, "vertexLit"))
			{
				stage->rgbGen = MaterialColorGen::VertexLit;

				if (stage->alphaGen == MaterialAlphaGen::Identity)
					stage->alphaGen = MaterialAlphaGen::Vertex;
			}
			else if (!util::Stricmp(token, "exactVertexLit"))
			{
				stage->rgbGen = MaterialColorGen::ExactVertexLit;
			}
			else if (!util::Stricmp(token, "lightingDiffuse"))
			{
				stage->rgbGen = MaterialColorGen::LightingDiffuse;
			}
			else if (!util::Stricmp(token, "oneMinusVertex"))
			{
				stage->rgbGen = MaterialColorGen::OneMinusVertex;
			}
			else
			{
				interface::PrintWarningf("WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, name);
			}
		}
		// alphaGen 
		else if (!util::Stricmp(token, "alphaGen"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing parameters for alphaGen in shader '%s'\n", name);
			}
			else if (!util::Stricmp(token, "wave"))
			{
				stage->alphaWave = parseWaveForm(text);
				stage->alphaGen = MaterialAlphaGen::Waveform;
			}
			else if (!util::Stricmp(token, "const"))
			{
				token = util::Parse(text, false);
				stage->constantColor.a = (float)atof(token);
				stage->alphaGen = MaterialAlphaGen::Const;
			}
			else if (!util::Stricmp(token, "identity"))
			{
				stage->alphaGen = MaterialAlphaGen::Identity;
			}
			else if (!util::Stricmp(token, "entity"))
			{
				stage->alphaGen = MaterialAlphaGen::Entity;
			}
			else if (!util::Stricmp(token, "oneMinusEntity"))
			{
				stage->alphaGen = MaterialAlphaGen::OneMinusEntity;
			}
			else if (!util::Stricmp(token, "vertex"))
			{
				stage->alphaGen = MaterialAlphaGen::Vertex;
			}
			else if (!util::Stricmp(token, "lightingSpecular"))
			{
				stage->alphaGen = MaterialAlphaGen::LightingSpecular;
			}
			else if (!util::Stricmp(token, "oneMinusVertex"))
			{
				stage->alphaGen = MaterialAlphaGen::OneMinusVertex;
			}
			else if (!util::Stricmp(token, "portal"))
			{
				stage->alphaGen = MaterialAlphaGen::Portal;
				token = util::Parse(text, false);

				if (token[0] == 0)
				{
					interface::PrintWarningf("WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to %g\n", name, portalRange);
				}
				else
				{
					portalRange = (float)atof(token);
				}
			}
			else
			{
				interface::PrintWarningf("WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, name);
				continue;
			}
		}
		// tcGen <function>
		else if (!util::Stricmp(token, "texgen") || !util::Stricmp(token, "tcGen")) 
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing texgen parm in shader '%s'\n", name);
			}
			else if (!util::Stricmp(token, "environment"))
			{
				stage->bundles[0].tcGen = MaterialTexCoordGen::EnvironmentMapped;
			}
			else if (!util::Stricmp(token, "lightmap"))
			{
				stage->bundles[0].tcGen = MaterialTexCoordGen::Lightmap;
			}
			else if (!util::Stricmp(token, "texture") || !util::Stricmp(token, "base"))
			{
				stage->bundles[0].tcGen = MaterialTexCoordGen::Texture;
			}
			else if (!util::Stricmp(token, "vector"))
			{
				stage->bundles[0].tcGenVectors[0] = parseVector(text);
				stage->bundles[0].tcGenVectors[1] = parseVector(text);
				stage->bundles[0].tcGen = MaterialTexCoordGen::Vector;
			}
			else 
			{
				interface::PrintWarningf("WARNING: unknown texgen parm in shader '%s'\n", name);
			}
		}
		// tcMod <type> <...>
		else if (!util::Stricmp(token, "tcMod"))
		{
			if (stage->bundles[0].numTexMods == MaterialTextureBundle::maxTexMods)
			{
				interface::PrintWarningf("WARNING: too many tcMod stages in shader '%s'", name);
				continue;
			}

			char buffer[1024] = "";

			while (1)
			{
				token = util::Parse(text, false);

				if (token[0] == 0)
					break;

				util::Strcat(buffer, sizeof (buffer), token);
				util::Strcat(buffer, sizeof (buffer), " ");
			}

			stage->bundles[0].texMods[stage->bundles[0].numTexMods] = parseTexMod(buffer);
			stage->bundles[0].numTexMods++;
		}
		// depthmask
		else if (!util::Stricmp(token, "depthwrite"))
		{
			stage->depthWrite = true;
			depthWriteExplicit = true;
		}
		else
		{
			interface::PrintWarningf("WARNING: unknown parameter '%s' in shader '%s'\n", token, name);
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
	char *token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.func = genFuncFromName(token);

	// BASE, AMP, PHASE, FREQ
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.base = (float)atof(token);
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.amplitude = (float)atof(token);
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.phase = (float)atof(token);
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("WARNING: missing waveform parm in shader '%s'\n", name);
		return wave;
	}

	wave.frequency = (float)atof(token);
	return wave;
}

MaterialTexModInfo Material::parseTexMod(char *buffer) const
{
	MaterialTexModInfo tmi;
	char **text = &buffer;
	char *token = util::Parse(text, false);

	if (!util::Stricmp(token, "turb"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing tcMod turb parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.base = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing tcMod turb in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.amplitude = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing tcMod turb in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.phase = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing tcMod turb in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.frequency = (float)atof(token);
		tmi.type = MaterialTexMod::Turbulent;
	}
	else if (!util::Stricmp(token, "scale"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing scale parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.scale[0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing scale parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.scale[1] = (float)atof(token);
		tmi.type = MaterialTexMod::Scale;
	}
	else if (!util::Stricmp(token, "scroll"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing scale scroll parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.scroll[0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing scale scroll parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.scroll[1] = (float)atof(token);
		tmi.type = MaterialTexMod::Scroll;
	}
	else if (!util::Stricmp(token, "stretch"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.func = genFuncFromName(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.base = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.amplitude = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.phase = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing stretch parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.wave.frequency = (float)atof(token);
		tmi.type = MaterialTexMod::Stretch;
	}
	else if (!util::Stricmp(token, "transform"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.matrix[0][0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.matrix[0][1] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.matrix[1][0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.matrix[1][1] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.translate[0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing transform parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.translate[1] = (float)atof(token);
		tmi.type = MaterialTexMod::Transform;
	}
	else if (!util::Stricmp(token, "rotate"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing tcMod rotate parms in shader '%s'\n", name);
			return tmi;
		}

		tmi.rotateSpeed = (float)atof(token);
		tmi.type = MaterialTexMod::Rotate;
	}
	else if (!util::Stricmp(token, "entityTranslate"))
	{
		tmi.type = MaterialTexMod::EntityTranslate;
	}
	else
	{
		interface::PrintWarningf("WARNING: unknown tcMod '%s' in shader '%s'\n", token, name);
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
	char *token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("WARNING: missing deform parm in shader '%s'\n", name);
	}
	else if (!util::Stricmp(token, "projectionShadow"))
	{
		ds.deformation = MaterialDeform::ProjectionShadow;
	}
	else if (!util::Stricmp(token, "autosprite"))
	{
		ds.deformation = MaterialDeform::Autosprite;
	}
	else if (!util::Stricmp(token, "autosprite2"))
	{
		ds.deformation = MaterialDeform::Autosprite2;
	}
	else if (!util::Stricmpn(token, "text", 4))
	{
		int n = token[4] - '0';

		if (n < 0 || n > 7)
			n = 0;

		ds.deformation = MaterialDeform((int)MaterialDeform::Text0 + n);
	}
	else if (!util::Stricmp(token, "bulge"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing deformVertexes bulge parm in shader '%s'\n", name);
			return ds;
		}

		ds.bulgeWidth = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing deformVertexes bulge parm in shader '%s'\n", name);
			return ds;
		}

		ds.bulgeHeight = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing deformVertexes bulge parm in shader '%s'\n", name);
			return ds;
		}

		ds.bulgeSpeed = (float)atof(token);
		ds.deformation = MaterialDeform::Bulge;
	}
	else if (!util::Stricmp(token, "wave"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing deformVertexes parm in shader '%s'\n", name);
			return ds;
		}

		if (atof(token) != 0)
		{
			ds.deformationSpread = 1.0f / (float)atof(token);
		}
		else
		{
			ds.deformationSpread = 100.0f;
			interface::PrintWarningf("WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", name);
		}

		ds.deformationWave = parseWaveForm(text);
		ds.deformation = MaterialDeform::Wave;
	}
	else if (!util::Stricmp(token, "normal"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing deformVertexes parm in shader '%s'\n", name);
			return ds;
		}

		ds.deformationWave.amplitude = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("WARNING: missing deformVertexes parm in shader '%s'\n", name);
			return ds;
		}

		ds.deformationWave.frequency = (float)atof(token);
		ds.deformation = MaterialDeform::Normals;
	}
	else if (!util::Stricmp(token, "move"))
	{
		for (size_t i = 0; i < 3; i++)
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("WARNING: missing deformVertexes parm in shader '%s'\n", name);
				return ds;
			}

			ds.moveVector[i] = (float)atof(token);
		}

		ds.deformationWave = parseWaveForm(text);
		ds.deformation = MaterialDeform::Move;
	}
	else
	{
		interface::PrintWarningf("WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, name);
	}

	return ds;
}

// skyParms <outerbox> <cloudheight> <innerbox>
void Material::parseSkyParms(char **text)
{
	const char * const suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	const int imgFlags = TextureFlags::Mipmap | TextureFlags::Picmip;

	// outerbox
	char *token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("WARNING: 'skyParms' missing parameter in shader '%s'\n", name);
		return;
	}

	if (strcmp(token, "-"))
	{
		for (size_t i = 0; i < 6; i++)
		{
			char pathname[MAX_QPATH];
			util::Sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
			sky.outerbox[i] = Texture::find(pathname, imgFlags | TextureFlags::ClampToEdge);

			if (!sky.outerbox[i])
			{
				sky.outerbox[i] = Texture::getDefault();
			}
		}
	}

	// cloudheight
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("WARNING: 'skyParms' missing parameter in shader '%s'\n", name);
		return;
	}

	sky.cloudHeight = (float)atof(token);

	if (!sky.cloudHeight)
	{
		sky.cloudHeight = 512;
	}

	Sky_InitializeTexCoords(sky.cloudHeight);

	// innerbox
	token = util::Parse(text, false);

	if (token[0] == 0) 
	{
		interface::PrintWarningf("WARNING: 'skyParms' missing parameter in shader '%s'\n", name);
		return;
	}

	if (strcmp(token, "-"))
	{
		for (size_t i=0 ; i<6 ; i++) 
		{
			char pathname[MAX_QPATH];
			util::Sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
			sky.innerbox[i] = Texture::find(pathname, imgFlags);

			if (!sky.innerbox[i])
			{
				sky.innerbox[i] = Texture::getDefault();
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
		interface::PrintWarningf("WARNING: invalid alphaFunc name '%s' in shader '%s'\n", name, this->name);
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
		interface::PrintWarningf("WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, this->name);
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
		interface::PrintWarningf("WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, this->name);
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
		interface::PrintWarningf("WARNING: invalid genfunc name '%s' in shader '%s'\n", name, this->name);
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
		value = (float)atof(name);
	}

	return value;
}

} // namespace renderer
