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

#if defined(ENGINE_IOQ3)
	{"metalsteps",	0,	SURF_METALSTEPS,0 },
	{"flesh",		0,	SURF_FLESH,		0 },
#endif
	{"nosteps",		0,	SURF_NOSTEPS,	0 },

	// drawsurf attributes
	{"nodraw",		0,	SURF_NODRAW,	0 },	// don't generate a drawsurface (or a lightmap)
	{"pointlight",	0,	SURF_POINTLIGHT, 0 },	// sample lighting at vertexes
	{"nolightmap",	0,	SURF_NOLIGHTMAP,0 },	// don't generate a lightmap
	{"nodlight",	0,	SURF_NODLIGHT, 0 },		// don't ever add dynamic lights

#if defined(ENGINE_IOQ3)
	{"dust",		0,	SURF_DUST, 0}			// leave a dust trail when walking on this surface
#endif
};

bool Material::parse(char **text)
{
	char *token = util::Parse(text, true);

	if (token[0] != '{')
	{
		interface::PrintWarningf("'%s': expecting '{', found '%s' instead\n", name, token);
		return false;
	}

	int stageIndex = 0;

	for (;;)
	{
		token = util::Parse(text, true);

		if (!token[0])
		{
			interface::PrintWarningf("'%s': no concluding '}'\n", name);
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
				interface::PrintWarningf("'%s': too many stages (max is %i)\n", name, (int)maxStages);
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
			sun.light = sun.light * (float)atof(token) * g_overbrightFactor / 255.0f;

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
				interface::PrintWarningf("'%s': max deforms\n", name);
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
		else if (!util::Stricmp(token, "q3map_surfacelight"))
		{
			token = util::Parse(text, false);
			surfaceLight = (float)atof(token);
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
		// character picmip adjustment
		else if (!util::Stricmp(token, "picmip2"))
		{
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
				return false;

			token = util::Parse(text, false);

			if (!token[0]) 
			{
				interface::PrintWarningf("'%s': missing fogParms 'distance to opaque'\n", name);
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
		// This is fixed fog for the skybox/clouds determined solely by the shader
		// it will not change in a level and will not be necessary
		// to force clients to use a sky fog the server says to.
		// skyfogvars <(r,g,b)> <dist>
		else if (!util::Stricmp(token, "skyfogvars"))
		{
			bool parsedFogColor;
			vec3 fogColor = parseVector(text, &parsedFogColor);

			if (!parsedFogColor)
				return false;

			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing density value for sky fog\n", name);
				continue;
			}

			if (atof(token) > 1)
			{
				interface::PrintWarningf("'%s': last value for skyfogvars is 'density' which needs to be 0.0-1.0\n", name);
				continue;
			}

			//R_SetFog(FOG_SKY, 0, 5, fogColor[0], fogColor[1], fogColor[2], atof(token));
			continue;
		}
		else if (!util::Stricmp(token, "sunshader"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing shader name for 'sunshader'\n", name);
				continue;
			}

			//tr.sunShaderName = "sun";
		}
		//----(SA)	added
		// ambient multiplier for lightgrid
		else if (!util::Stricmp(token, "lightgridmulamb"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing value for 'lightgrid ambient multiplier'\n", name);
				continue;
			}

			if (atof(token) > 0)
			{
				//tr.lightGridMulAmbient = atof(token);
			}
		}
		// directional multiplier for lightgrid
		else if (!util::Stricmp(token, "lightgridmuldir"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing value for 'lightgrid directional multiplier'\n", name);
				continue;
			}

			if (atof(token) > 0)
			{
				//tr.lightGridMulDirected = atof(token);
			}
		}
		//----(SA)	end
		else if (!util::Stricmp(token, "waterfogvars"))
		{
			bool waterParsed;
			vec3 waterColor = parseVector(text, &waterParsed);

			if (!waterParsed)
				return false;

			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing density/distance value for water fog\n", name);
				continue;
			}

			float fogvar = (float)atof(token);
			char fogString[64];

			//----(SA)	right now allow one water color per map.  I'm sure this will need
			//			to change at some point, but I'm not sure how to track fog parameters
			//			on a "per-water volume" basis yet.
			// '0' specifies "use the map values for everything except the fog color
			if (fogvar == 0)
			{
				// TODO
			}
			// distance "linear" fog
			else if (fogvar > 1)
			{
				util::Sprintf(fogString, sizeof(fogString), "0 %d 1.1 %f %f %f 200", (int)fogvar, waterColor[0], waterColor[1], waterColor[2]);
			}
			// density "exp" fog
			else
			{
				util::Sprintf(fogString, sizeof(fogString), "0 5 %f %f %f %f 200", fogvar, waterColor[0], waterColor[1], waterColor[2]);
			}

			//		near
			//		far
			//		density
			//		r,g,b
			//		time to complete
			interface::Cvar_Set("r_waterFogColor", fogString);
			continue;
		}
		// fogvars
		else if (!util::Stricmp(token, "fogvars"))
		{
			bool parsedFogColor;
			vec3 fogColor = parseVector(text, &parsedFogColor);

			if (!parsedFogColor)
				return false;

			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing density value for the fog\n", name);
				continue;
			}

			//----(SA)	NOTE:	fogFar > 1 means the shader is setting the farclip, < 1 means setting
			//					density (so old maps or maps that just need softening fog don't have to care about farclip)
			float fogDensity = (float)atof(token);
			int fogFar;

			if (fogDensity >= 1)
			{
				// linear
				fogFar = (int)fogDensity;
			}
			else
			{
				fogFar = 5;
			}

			interface::Cvar_Set("r_mapFogColor", util::VarArgs("0 %d %f %f %f %f 0", fogFar, fogDensity, fogColor[0], fogColor[1], fogColor[2]));
			continue;
		}
		// Ridah, allow disable fog for some shaders
		else if (!util::Stricmp(token, "nofog"))
		{
			noFog = true;
		}
		// RF, allow each shader to permit compression if available
		else if (!util::Stricmp(token, "allowcompress"))
		{
		}
		else if (!util::Stricmp(token, "nocompress"))
		{
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
				interface::PrintWarningf("'%s': missing cull parms\n", name);
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
				interface::PrintWarningf("'%s': invalid cull parm '%s'\n", name, token);
			}
		}
		// sort
		else if (!util::Stricmp(token, "sort"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("'%s': missing sort parameter\n", name);
				continue;
			}

			sort = sortFromName(token);
		}
		else
		{
			interface::PrintWarningf("'%s': unknown general shader parameter '%s'\n", name, token);
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
		interface::PrintWarningf("'%s': missing opening parenthesis\n", name);

		if (result)
			*result = false;

		return v;
	}

	for (size_t i = 0; i < 3; i++)
	{
		token = util::Parse(text, false);

		if (!token[0])
		{
			interface::PrintWarningf("'%s': missing vector element\n", name);

			if (result)
				*result = false;

			return v;
		}

		v[i] = (float)atof(token);
	}

	token = util::Parse(text, false);

	if (strcmp(token, ")"))
	{
		interface::PrintWarningf("'%s': missing closing parenthesis\n", name);

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
		const char *token = util::Parse(text, true);

		if (!token[0])
		{
			interface::PrintWarningf("'%s': no matching '}' found\n", name);
			return false;
		}

		if (token[0] == '}')
			break;

		// check special case for map16/map32/mapcomp/mapnocomp (compression enabled)
		// only use this texture if 16 bit color depth
		if (!util::Stricmp(token, "map16"))
		{
			util::Parse(text, false); // ignore the map
			continue;
		}
		else if (!util::Stricmp(token, "map32"))
		{
			token = "map";   // use this map
		}
		// only use this texture if compression is enabled
		else if (!util::Stricmp(token, "mapcomp"))
		{
			/*if (glConfig.textureCompression && r_ext_compressed_textures->integer)
			{
				token = "map";   // use this map
			}
			else*/
			{
				util::Parse(text, false);   // ignore the map
				continue;
			}
		}
		// only use this texture if compression is not available or disabled
		else if (!util::Stricmp(token, "mapnocomp"))
		{
			//if (!glConfig.textureCompression)
			{
				token = "map";   // use this map
			}
			/*else
			{
				util::Parse(text, false);   // ignore the map
				continue;
			}*/
		}
		// only use this texture if compression is enabled
		else if (!util::Stricmp(token, "animmapcomp"))
		{
			/*if (glConfig.textureCompression && r_ext_compressed_textures->integer)
			{
				token = "animmap";   // use this map
			}
			else*/
			{
				while (token[0])
					util::Parse(text, false);   // ignore the map

				continue;
			}
		}
		// only use this texture if compression is not available or disabled
		else if (!util::Stricmp(token, "animmapnocomp"))
		{
			//if (!glConfig.textureCompression)
			{
				token = "animmap";   // use this map
			}
			/*else
			{
				while (token[0])
					util::Parse(text, false);   // ignore the map

				continue;
			}*/
		}
		
		// map <name>
		if (!util::Stricmp(token, "map"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing parameter for 'map' keyword\n", name);
				return false;
			}

			if (!util::Stricmp(token, "$whiteimage"))
			{
				stage->bundles[0].textures[0] = g_textureCache->getWhite();
			}
			else if (!util::Stricmp(token, "$lightmap"))
			{
				stage->bundles[0].isLightmap = true;
				const Texture *lightmap = world::IsLoaded() ? world::GetLightmap(lightmapIndex) : nullptr;

				if (!lightmap)
				{
					lightmap = g_textureCache->getWhite();
				}
				
				stage->bundles[0].textures[0] = lightmap;
			}
			else if (!util::Stricmp(token, "$deluxemap"))
			{
				/*if (!tr.worldDeluxeMapping)
				{
					interface::PrintWarningf("'%s': wants a deluxe map in a map compiled without them\n", name);
					return false;
				}*/

				stage->bundles[0].isLightmap = true;
				stage->bundles[0].textures[0] = g_textureCache->getWhite();

				/*if (lightmapIndex < 0)
				{
					stage->bundles[0].textures[0] = g_textureCache->getWhite();
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

				stage->bundles[0].textures[0] = g_textureCache->find(token, flags);

				if (!stage->bundles[0].textures[0])
				{
					interface::PrintWarningf("'%s': could not find texture '%s'\n", name, token);
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
				interface::PrintWarningf("'%s': missing parameter for 'clampmap' keyword\n", name);
				return false;
			}

			if (!noMipMaps)
				flags |= TextureFlags::Mipmap;

			if (!noPicMip)
				flags |= TextureFlags::Picmip;

			stage->bundles[0].textures[0] = g_textureCache->find(token, flags);

			if (!stage->bundles[0].textures[0])
			{
				interface::PrintWarningf("'%s': could not find texture '%s'\n", name, token);
				return false;
			}
		}
		// animMap <frequency> <image1> .... <imageN>
		else if (!util::Stricmp(token, "animMap"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing parameter for 'animMap' keyword\n", name);
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

					stage->bundles[0].textures[num] = g_textureCache->find(token, flags);

					if (!stage->bundles[0].textures[num])
					{
						interface::PrintWarningf("'%s': could not find texture '%s'\n", name, token);
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
				interface::PrintWarningf("'%s': missing parameter for 'videoMap' keyword\n", name);
				return false;
			}

			stage->bundles[0].videoMapHandle = interface::CIN_PlayCinematic(token, 0, 0, 256, 256);

			if (stage->bundles[0].videoMapHandle != -1)
			{
				stage->bundles[0].isVideoMap = true;
				stage->bundles[0].textures[0] = g_textureCache->getScratch(size_t(stage->bundles[0].videoMapHandle));
			}
		}
		// alphafunc <func>
		else if (!util::Stricmp(token, "alphaFunc"))
		{
			token = util::Parse(text, false);

			if (!token[0])
			{
				interface::PrintWarningf("'%s': missing parameter for 'alphaFunc' keyword\n", name);
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
				interface::PrintWarningf("'%s': missing parameter for 'depthfunc' keyword\n", name);
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
				interface::PrintWarningf("'%s': unknown depthfunc '%s'\n", name, token);
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
				interface::PrintWarningf("'%s': missing parm for blendFunc\n", name);
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
					interface::PrintWarningf("'%s': missing parm for blendFunc\n", name);
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
				interface::PrintWarningf("'%s': missing parameters for stage\n", name);
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
				interface::PrintWarningf("'%s': unknown stage parameter '%s'\n", name, token);
			}
		}
		// specularReflectance <value>
		else if (!util::Stricmp(token, "specularreflectance"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("'%s': missing parameter for specular reflectance\n", name);
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
				interface::PrintWarningf("'%s': missing parameter for specular exponent\n", name);
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
				interface::PrintWarningf("'%s': missing parameter for gloss\n", name);
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
				interface::PrintWarningf("'%s': missing parameter for parallaxDepth\n", name);
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
				interface::PrintWarningf("'%s': missing parameter for normalScale\n", name);
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
				interface::PrintWarningf("'%s': missing parameter for specularScale\n", name);
				continue;
			}

			stage->specularScale.r = (float)atof(token);
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("'%s': missing parameter for specularScale\n", name);
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
				interface::PrintWarningf("'%s': missing parameters for rgbGen\n", name);
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
				interface::PrintWarningf("'%s': unknown rgbGen parameter '%s'\n", name, token);
			}
		}
		// alphaGen 
		else if (!util::Stricmp(token, "alphaGen"))
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("'%s': missing parameters for alphaGen\n", name);
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
			// Ridah
			else if (!util::Stricmp(token, "normalzfade"))
			{
				stage->alphaGen = MaterialAlphaGen::NormalZFade;
				token = util::Parse(text, false);

				if (token[0])
				{
					stage->constantColor[3] = 255 * (float)atof(token);
				}
				else
				{
					stage->constantColor[3] = 255;
				}

				token = util::Parse(text, false);

				if (token[0])
				{
					stage->zFadeBounds[0] = (float)atof(token); // lower range
					token = util::Parse(text, false);
					stage->zFadeBounds[1] = (float)atof(token); // upper range
				}
				else
				{
					stage->zFadeBounds[0] = -1.0; // lower range
					stage->zFadeBounds[1] = 1.0; // upper range
				}

			}
			// done.
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
					interface::PrintWarningf("'%s': missing range parameter for alphaGen portal, defaulting to %g\n", name, portalRange);
				}
				else
				{
					portalRange = (float)atof(token);
				}
			}
			else
			{
				interface::PrintWarningf("'%s': unknown alphaGen parameter '%s'\n", name, token);
				continue;
			}
		}
		// tcGen <function>
		else if (!util::Stricmp(token, "texgen") || !util::Stricmp(token, "tcGen")) 
		{
			token = util::Parse(text, false);

			if (token[0] == 0)
			{
				interface::PrintWarningf("'%s': missing texgen parm\n", name);
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
				interface::PrintWarningf("'%s': unknown texgen parm\n", name);
			}
		}
		// tcMod <type> <...>
		else if (!util::Stricmp(token, "tcMod"))
		{
			if (stage->bundles[0].numTexMods == MaterialTextureBundle::maxTexMods)
			{
				interface::PrintWarningf("'%s': too many tcMod stages", name);
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
			interface::PrintWarningf("'%s': unknown parameter '%s'\n", name, token);
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
		interface::PrintWarningf("'%s': missing waveform parm\n", name);
		return wave;
	}

	wave.func = genFuncFromName(token);

	// BASE, AMP, PHASE, FREQ
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("'%s': missing waveform parm\n", name);
		return wave;
	}

	wave.base = (float)atof(token);
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("'%s': missing waveform parm\n", name);
		return wave;
	}

	wave.amplitude = (float)atof(token);
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("'%s': missing waveform parm\n", name);
		return wave;
	}

	wave.phase = (float)atof(token);
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("'%s': missing waveform parm\n", name);
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
			interface::PrintWarningf("'%s': missing tcMod turb parms\n", name);
			return tmi;
		}

		tmi.wave.base = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing tcMod turb\n", name);
			return tmi;
		}

		tmi.wave.amplitude = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing tcMod turb\n", name);
			return tmi;
		}

		tmi.wave.phase = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing tcMod turb\n", name);
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
			interface::PrintWarningf("'%s': missing scale parms\n", name);
			return tmi;
		}

		tmi.scale[0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing scale parms\n", name);
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
			interface::PrintWarningf("'%s': missing scale scroll parms\n", name);
			return tmi;
		}

		tmi.scroll[0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing scale scroll parms\n", name);
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
			interface::PrintWarningf("'%s': missing stretch parms\n", name);
			return tmi;
		}

		tmi.wave.func = genFuncFromName(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing stretch parms\n", name);
			return tmi;
		}

		tmi.wave.base = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing stretch parms\n", name);
			return tmi;
		}

		tmi.wave.amplitude = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing stretch parms\n", name);
			return tmi;
		}

		tmi.wave.phase = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing stretch parms\n", name);
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
			interface::PrintWarningf("'%s': missing transform parms\n", name);
			return tmi;
		}

		tmi.matrix[0][0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing transform parms\n", name);
			return tmi;
		}

		tmi.matrix[0][1] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing transform parms\n", name);
			return tmi;
		}

		tmi.matrix[1][0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing transform parms\n", name);
			return tmi;
		}

		tmi.matrix[1][1] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing transform parms\n", name);
			return tmi;
		}

		tmi.translate[0] = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing transform parms\n", name);
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
			interface::PrintWarningf("'%s': missing tcMod rotate parms\n", name);
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
		interface::PrintWarningf("'%s': unknown tcMod '%s'\n", name, token);
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
		interface::PrintWarningf("'%s': missing deform parm\n", name);
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
			interface::PrintWarningf("'%s': missing deformVertexes bulge parm\n", name);
			return ds;
		}

		ds.bulgeWidth = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing deformVertexes bulge parm\n", name);
			return ds;
		}

		ds.bulgeHeight = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing deformVertexes bulge parm\n", name);
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
			interface::PrintWarningf("'%s': missing deformVertexes parm\n", name);
			return ds;
		}

		if (atof(token) != 0)
		{
			ds.deformationSpread = 1.0f / (float)atof(token);
		}
		else
		{
			ds.deformationSpread = 100.0f;
			interface::PrintWarningf("'%s': illegal div value of 0 in deformVertexes command\n", name);
		}

		ds.deformationWave = parseWaveForm(text);
		ds.deformation = MaterialDeform::Wave;
	}
	else if (!util::Stricmp(token, "normal"))
	{
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing deformVertexes parm\n", name);
			return ds;
		}

		ds.deformationWave.amplitude = (float)atof(token);
		token = util::Parse(text, false);

		if (token[0] == 0)
		{
			interface::PrintWarningf("'%s': missing deformVertexes parm\n", name);
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
				interface::PrintWarningf("'%s': missing deformVertexes parm\n", name);
				return ds;
			}

			ds.moveVector[i] = (float)atof(token);
		}

		ds.deformationWave = parseWaveForm(text);
		ds.deformation = MaterialDeform::Move;
	}
	else
	{
		interface::PrintWarningf("'%s': unknown deformVertexes subtype '%s' found\n", name, token);
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
		interface::PrintWarningf("'%s': 'skyParms' missing parameter\n", name);
		return;
	}

	if (strcmp(token, "-"))
	{
		for (size_t i = 0; i < 6; i++)
		{
			char pathname[MAX_QPATH];
			util::Sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
			sky.outerbox[i] = g_textureCache->find(pathname, imgFlags | TextureFlags::ClampToEdge);

			if (!sky.outerbox[i])
			{
				sky.outerbox[i] = g_textureCache->getDefault();
			}
		}
	}

	// cloudheight
	token = util::Parse(text, false);

	if (token[0] == 0)
	{
		interface::PrintWarningf("'%s': 'skyParms' missing parameter\n", name);
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
		interface::PrintWarningf("'%s': 'skyParms' missing parameter\n", name);
		return;
	}

	if (strcmp(token, "-"))
	{
		for (size_t i=0 ; i<6 ; i++) 
		{
			char pathname[MAX_QPATH];
			util::Sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
			sky.innerbox[i] = g_textureCache->find(pathname, imgFlags);

			if (!sky.innerbox[i])
			{
				sky.innerbox[i] = g_textureCache->getDefault();
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
		interface::PrintWarningf("'%s': invalid alphaFunc name '%s'\n", this->name, name);
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
		interface::PrintWarningf("'%s': unknown blend mode '%s', substituting GL_ONE\n", this->name, name);
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
		interface::PrintWarningf("'%s': unknown blend mode '%s', substituting GL_ONE\n", this->name, name);
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
		interface::PrintWarningf("'%s': invalid genfunc name '%s'\n", this->name, name);
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
