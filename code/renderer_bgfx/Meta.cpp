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
namespace meta {

struct Meta
{
	Material *bfgExplosionMaterial = nullptr;
	Model *bfgMissibleModel = nullptr;
	Material *plasmaBallMaterial = nullptr;
	Material *plasmaExplosionMaterial = nullptr;
};

static Meta s_meta;

struct LerpTextureAnimationMaterial
{
	const char *name;
	MaterialStageTextureAnimationLerp lerp;
};

static const LerpTextureAnimationMaterial s_lerpTextureAnimationMaterials[] =
{
	{ "bfgExplosion", MaterialStageTextureAnimationLerp::Clamp },
	{ "bloodExplosion", MaterialStageTextureAnimationLerp::Clamp },
	{ "bulletExplosion", MaterialStageTextureAnimationLerp::Clamp },
	{ "grenadeExplosion", MaterialStageTextureAnimationLerp::Clamp },
	//{ "railExplosion", MaterialStageTextureAnimationLerp::Clamp },
	{ "rocketExplosion", MaterialStageTextureAnimationLerp::Clamp },
	{ "textures/sfx/flame1", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flame1dark", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flame1side", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flame1_hell", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flame2", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flameanim_dimmer", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flameanim_blue", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flameanim_blue_nolight", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flameanim_blue_pj", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flameanim_green_pj", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/flameanim_red_pj", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/xflame1side", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/xflame2_1800", MaterialStageTextureAnimationLerp::Wrap },
	{ "textures/sfx/x_conduit", MaterialStageTextureAnimationLerp::Wrap }
};

static const char *s_reflectiveMaterialNames[] =
{
	//"textures/liquids/clear_ripple1",
	"textures/liquids/calm_poollight", // q3dm2
	//"textures/liquids/clear_calm1"
};

static const char * s_bloomWhitelist[] =
{
	// Q3A
	"models/mapobjects/baph/bapholamp_fx*",
	"models/mapobjects/baph/wrist.*",
	"models/mapobjects/barrel/barrel2fx.*",
	"models/mapobjects/bitch/forearm0*",
	"models/mapobjects/bitch/hologirl2.*",
	"models/mapobjects/bitch/orb.*",
	"models/mapobjects/barrel/colua0/colua0_flare.*",
	"models/mapobjects/flares/*",
	"models/mapobjects/gratelamp/gratelamp_flare.*",
	"models/mapobjects/jets/jet_1.*",
	"models/mapobjects/jets/jet_as.*",
	"models/mapobjects/lamps/bot_flare*",
	"models/mapobjects/lamps/flare03.*",
	"models/mapobjects/podium/podiumfx1b.*",
	"models/mapobjects/podium/podskullfx.*",
	"models/mapobjects/portal_2/portal_3_edge_glo.*",
	"models/mapobjects/portal_2/portal_3_glo.*",
	"models/mapobjects/slamp/slamp3.*",
	"models/mapobjects/spotlamp/spotlamp_l.*",
	"models/players/doom/phobos_fx.*",
	"models/players/slash/slashskate.*",
	"models/weapons2/plasma/plasma_glo.*",
	"models/weapons2/railgun/railgun2.glow.*",
	//"models/mapobjects/teleporter/energy*",
	"models/mapobjects/teleporter/teleporter_edge2.*",
	"models/mapobjects/teleporter/transparency2.*",
	"textures/base_floor/tilefloor7_owfx.*",
	"textures/base_light/baslt4_1.blend.*",
	"textures/base_light/border7_ceil50glow.*",
	"textures/base_light/border11light.blend.*",
	"textures/base_light/ceil1_22a.blend.*",
	"textures/base_light/ceil1_30.blend.*",
	"textures/base_light/ceil1_34.blend.*",
	"textures/base_light/ceil1_37.blend.*",
	"textures/base_light/ceil1_4.blend.*",
	"textures/base_light/ceil1_38.blend.*",
	"textures/base_light/ceil1_39.blend.*",
	"textures/base_light/cornerlight.glow.*",
	"textures/base_light/geolight_glow.*",
	"textures/base_light/jaildr1_3.blend.*",
	"textures/base_light/jaildr03_2.blend.*",
	"textures/base_light/light1.blend.*",
	"textures/base_light/light1blue.blend.*",
	"textures/base_light/light1red.blend.*",
	"textures/base_light/light2.blend.*",
	"textures/base_light/patch10_pj_lite.blend.*",
	"textures/base_light/patch10_pj_lite2.blend.*",
	"textures/base_light/proto_light2.*",
	"textures/base_light/proto_lightred.*",
	"textures/base_light/runway_glow.*",
	"textures/base_light/trianglelight.blend.*",
	"textures/base_light/scrolllight2.*",
	"textures/base_light/wsupprt1_12.blend.*",
	"textures/base_light/xlight5.blend.*",
	"textures/base_trim/border11c_light.*",
	"textures/base_trim/border11c_pulse1b.*",
	"textures/base_trim/border11light.glow.*",
	"textures/base_trim/spiderbit_fx.*",
	"textures/base_trim/techborder_fx.*",
	"textures/base_wall/basewall01_owfx.*",
	"textures/base_wall/basewall01bitfx.*",
	"textures/base_wall/bluemetalsupport2bglow.*",
	"textures/base_wall/bluemetalsupport2clight.glow.*",
	"textures/base_wall/bluemetalsupport2eyel.glow.*",
	"textures/base_wall/bluemetalsupport2fline_glow.*",
	"textures/base_wall/bluemetalsupport2ftv_glow.*",
	"textures/base_wall/dooreyelight.*",
	//"textures/ctf/blue_telep*",
	//"textures/ctf/red_telep*",
	"textures/gothic_block/evil2cglow.*",
	"textures/gothic_block/evil2ckillblockglow.*",
	"textures/gothic_block/killblock_i4glow.*",
	"textures/gothic_light/border7_ceil39b.blend.*",
	"textures/gothic_light/border_ceil39.blend.*",
	"textures/gothic_light/gothic_light2_blend.*",
	"textures/gothic_light/ironcrosslt2.blend.*",
	"textures/gothic_light/pentagram_light1_blend.*",
	"textures/effects/envmapbfg.*",
	"textures/liquids/bubbles.*",
	"textures/liquids/lavahell.*",
	"textures/liquids/protolava*",
	"textures/liquids/slime7*",
	"textures/sfx/compscreen/*",
	"textures/sfx/b_flame*",
	"textures/sfx/bouncepad01b_layer1.*",
	"textures/sfx/demonltblackfinal_glow2.*",
	"textures/sfx/electric2.blend.*",
	"textures/sfx/electricgrade3.*",
	"textures/sfx/electricslime*",
	"textures/sfx/fire_ctf*",
	"textures/sfx/firegorre.*",
	"textures/sfx/firegorre2.*",
	"textures/sfx/fireswirl2.*",
	"textures/sfx/fireswirl2blue.*",
	"textures/sfx/firewalla.*",
	"textures/sfx/flame*",
	"textures/sfx/g_flame*",
	"textures/sfx/hologirl.*",
	"textures/sfx/jacobs_x.*",
	"textures/sfx/jumppadsmall.*",
	"textures/sfx/launchpad_arrow.*",
	"textures/sfx/launchpad_dot.*",
	"textures/sfx/metalfloor_wall_5bglowblu.*",
	"textures/sfx/metalfloor_wall_9bglow.*",
	"textures/sfx/metalfloor_wall_14bglow2.*",
	"textures/sfx/pentagramfloor_red_glow.*",
	"textures/sfx/portal_sfx1.*", // Fixes bloom through textures/sfx/portal_sfx portal effect.
	"textures/sfx/portal_sfx_ring_electric.*",
	"textures/sfx/proto_zzzt*",
	"textures/sfx/r_flame*",
	"textures/sfx/surface6jumppad.blend.*",
	"textures/sfx/tesla1.*",
	"textures/sfx/tesla1b.*",
	"textures/sfx/x_conduit2.*",
	"textures/sfx/x_conduit3.*",
	"textures/sfx/xian_dm3padwallglow.*",
	"textures/sfx/zap_scroll.*",
	"textures/sfx/zap_scroll2.*",
	"textures/skies/*",

	// Team Arena
	"models/mapobjects/gratelamp/lightbulb.*",
	"models/mapobjects/slamp/slamp3*",
	"models/mapobjects/spawn/spawn3_*",
	"models/mapobjects/techlamp/techlamp_pole2.*",
	"models/mapobjects/xlamp/xlamp_blue.*",
	"models/mapobjects/xlamp/xlamp_ntrl.*",
	"models/mapobjects/xlamp/xlamp_red.*",
	"team_icon/*",
	"textures/base_object/boxQ3_2.blend.*",
	"textures/base_wall2/blue_arrow_small.*",
	"textures/base_wall2/blue_circle.*",
	"textures/base_wall2/blue_line_glow.*",
	"textures/base_wall2/blue_solid.*",
	"textures/base_wall2/bluearrows.*",
	"textures/base_wall2/jumppad_shadow.*",
	"textures/base_wall2/red_arrow_small.*",
	"textures/base_wall2/red_circle.*",
	"textures/base_wall2/red_line_glow.*",
	"textures/base_wall2/red_solid.*",
	"textures/base_wall2/redarrows.*",
	"textures/base_wall2/techfloor_kc_shadow.*",
	"textures/base_wall2/zzztblue_kc.*",
	"textures/base_wall2/zzztntrl_kc.*",
	"textures/base_wall2/zzztred_kc.*",
	"textures/ctf2/blueteam01.*",
	"textures/ctf2/redteam01.*",
	"textures/ctf2/jaildr_blue.blend.*",
	"textures/ctf2/pj_baseboardb_l.*",
	"textures/ctf2/pj_baseboardr_l.*",
	"textures/proto2/lightbulb.*",
	"textures/proto2/marble02btrim03_lt.*",
	"textures/proto2/marble02rtrim03_lt.*",
	"textures/proto2/stadlight01fx.*",
	"textures/sfx2/b_smack*",
	"textures/sfx2/jumpadb.*",
	"textures/sfx2/blaunch*",
	"textures/sfx2/dm3padwallglow*",
	"textures/sfx2/jumpadb2.*",
	"textures/sfx2/jumpadn2.*",
	"textures/sfx2/jumpadr*",
	"textures/sfx2/nlaunch*",
	"textures/sfx2/r_fight0*",
	"textures/sfx2/rlaunch*",
	"textures/sfx2/swirl_b*",
	"textures/sfx2/swirl_r*"
};

static const char * s_textureVariationWhitelist[] =
{
	// q3dm7
	"textures/organics/dirt",
	"textures/organics/dirt2",
	"textures/organics/dirt_trans",
	// q3dm16
	"textures/base_wall/metalfloor_wall_14_specular",
	// q3dm17
	"textures/base_wall/metalfloor_wall_15",
	// q3dm18
	"textures/base_wall/metalfloor_wall_11",
	// q3dm19
	"textures/base_floor/metaltechfloor01final"
};

void Initialize()
{
	s_meta = Meta();
}

static float CalculateExplosionLight(float entityShaderTime, float durationMilliseconds)
{
	// From CG_AddExplosion
	float light = (main::GetFloatTime() - entityShaderTime) / (durationMilliseconds / 1000.0f);

	if (light < 0.5f)
		return 1.0f;

	return 1.0f - (light - 0.5f) * 2.0f;
}

void OnEntityAddedToScene(const Entity &entity, bool isWorldScene)
{
	if (!isWorldScene || !main::AreExtraDynamicLightsEnabled())
		return;

	// Hack in extra dlights for Quake 3 content.
	const vec3 bfgColor = util::ToLinear(vec3(0.08f, 1.0f, 0.4f));
	const vec3 lightningColor = util::ToLinear(vec3(0.6f, 0.6f, 1));
	const vec3 plasmaColor = util::ToLinear(vec3(0.6f, 0.6f, 1.0f));
	DynamicLight dl;
	dl.color_radius = vec4::empty;
	dl.position_type = vec4(entity.position, DynamicLight::Point);

	// BFG projectile.
	if (entity.type == EntityType::Model && s_meta.bfgMissibleModel && entity.handle == s_meta.bfgMissibleModel->getIndex())
	{
		dl.color_radius = vec4(bfgColor, 200); // Same radius as rocket.
	}
	// BFG explosion.
	else if (entity.type == EntityType::Sprite && s_meta.bfgExplosionMaterial && entity.customMaterial == s_meta.bfgExplosionMaterial->index)
	{
		// Same radius as rocket explosion. CG_MissileHitWall: 600ms duration.
		dl.color_radius = vec4(bfgColor, 300 * CalculateExplosionLight(entity.materialTime, 600));
	}
	// Lightning bolt.
	else if (entity.type == EntityType::Lightning)
	{
		const float base = 1;
		const float amplitude = 0.1f;
		const float phase = 0;
		const float freq = 10.1f;
		const float radius = base + g_sinTable[std::lrintf((phase + main::GetFloatTime() * freq) * g_funcTableSize) & g_funcTableMask] * amplitude;
		dl.capsuleEnd = vec3(entity.oldPosition);
		dl.color_radius = vec4(lightningColor, 200 * radius);
		dl.position_type.w = DynamicLight::Capsule;
	}
	// Plasma ball.
	else if (entity.type == EntityType::Sprite && s_meta.plasmaBallMaterial && entity.customMaterial == s_meta.plasmaBallMaterial->index)
	{
		dl.color_radius = vec4(plasmaColor, 150);
	}
	// Plasma explosion.
	else if (entity.type == EntityType::Model && s_meta.plasmaExplosionMaterial && entity.customMaterial == s_meta.plasmaExplosionMaterial->index)
	{
		// CG_MissileHitWall: 600ms duration.
		dl.color_radius = vec4(plasmaColor, 200 * CalculateExplosionLight(entity.materialTime, 600));
	}
	// Rail core.
	else if (entity.type == EntityType::RailCore)
	{
		dl.capsuleEnd = vec3(entity.oldPosition);
		dl.color_radius = vec4(util::ToLinear(entity.materialColor.rgb()), 200);
		dl.position_type.w = DynamicLight::Capsule;
	}

	if (dl.color_radius.a > 0)
	{
		main::AddDynamicLightToScene(dl);
	}
}

void OnMaterialCreate(Material *material)
{
	if (!util::Stricmp(material->name, "bfgExplosion"))
	{
		s_meta.bfgExplosionMaterial = material;
	}
	else if (!util::Stricmp(material->name, "sprites/plasma1"))
	{
		s_meta.plasmaBallMaterial = material;
	}
	else if (!util::Stricmp(material->name, "plasmaExplosion"))
	{
		s_meta.plasmaExplosionMaterial = material;
	}

	// Enable texture variation.
	bool textureVariation = false;

	for (int k = 0; k < BX_COUNTOF(s_textureVariationWhitelist); k++)
	{
		const char *entry = s_textureVariationWhitelist[k];
		const char *wildcard = strstr(entry, "*");

		if (!util::Stricmp(material->name, entry) || (wildcard && !util::Stricmpn(material->name, entry, int(wildcard - entry))))
		{
			textureVariation = true;
		}
	}

	for (int i = 0; i < Material::maxStages; i++)
	{
		MaterialStage &stage = material->stages[i];

		if (!stage.active)
			break;

		for (int j = 0; j < MaterialTextureBundle::maxImageAnimations; j++)
		{
			const Texture *texture = stage.bundles[MaterialTextureBundleIndex::DiffuseMap].textures[j];

			if (!texture)
				break;

			// Enable bloom.
			for (int k = 0; k < BX_COUNTOF(s_bloomWhitelist); k++)
			{
				const char *entry = s_bloomWhitelist[k];
				const char *wildcard = strstr(entry, "*");

				if (!util::Stricmp(texture->getName(), entry) || (wildcard && !util::Stricmpn(texture->getName(), entry, int(wildcard - entry))))
				{
					stage.bloom = true;
				}
			}

			stage.textureVariation = textureVariation;
		}
	}

	if (main::IsLerpTextureAnimationEnabled())
	{
		for (const LerpTextureAnimationMaterial &m : s_lerpTextureAnimationMaterials)
		{
			if (util::Stricmp(material->name, m.name) != 0)
				continue;

			for (MaterialStage &stage : material->stages)
			{
				if (!stage.active)
					continue;

				for (const MaterialTextureBundle bundle : stage.bundles)
				{
					if (bundle.numImageAnimations > 1)
					{
						stage.textureAnimationLerp = m.lerp;
						break;
					}
				}

				// Waveforms are used to hide the default choppy animations. They make the lerped animations flicker, so disable them.
				if (stage.rgbGen == MaterialColorGen::Waveform)
					stage.rgbGen = MaterialColorGen::Identity;
			}
		}
	}

	if (main::AreWaterReflectionsEnabled())
	{
		for (size_t i = 0; i < BX_COUNTOF(s_reflectiveMaterialNames); i++)
		{
			if (util::Stricmp(material->name, s_reflectiveMaterialNames[i]) != 0)
				continue;

			// Use the existing material as the reflective back side, i.e. what you see when under the water plane.
			material->cullType = MaterialCullType::BackSided;
			material->reflective = MaterialReflective::BackSide;

			// Create a copy of this material to use for the reflective front side, i.e. what you see when above the water plane - the surface that displays the reflection.
			// Insert a reflection stage at index 0.
			Material reflection = *material;
			util::Strncpyz(reflection.name, util::VarArgs("%s/reflection", material->name), sizeof(reflection.name));
			reflection.cullType = MaterialCullType::FrontSided;
			reflection.reflective = MaterialReflective::FrontSide;

			for (int i = Material::maxStages - 1; i > 0; i--)
			{
				MaterialStage *stage = &reflection.stages[i];
				MaterialStage *prevStage = &reflection.stages[i - 1];

				if (prevStage->active)
					*stage = *prevStage;
			}

			MaterialStage *stage = &reflection.stages[0];
			*stage = MaterialStage();
			stage->active = true;
			stage->depthWrite = false;
			stage->bundles[0].textures[0] = g_textureCache->find("*reflection");
			stage->bundles[0].tcGen = MaterialTexCoordGen::Fragment;
			stage->blendSrc = BGFX_STATE_BLEND_SRC_ALPHA;
			stage->blendDst = BGFX_STATE_BLEND_INV_SRC_ALPHA;
			stage->rgbGen = MaterialColorGen::Identity;
			stage->alphaGen = MaterialAlphaGen::Water;
			material->reflectiveFrontSideMaterial = g_materialCache->createMaterial(reflection);
		}
	}
}

void OnModelCreate(Model *model)
{
	if (!util::Stricmp(model->getName(), "models/weaphits/bfg.md3"))
	{
		s_meta.bfgMissibleModel = model;
	}
}

} // namespace meta
} // namespace renderer
