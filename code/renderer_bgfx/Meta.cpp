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
	"textures/liquids/clear_ripple1",
	"textures/liquids/calm_poollight",
	"textures/liquids/clear_calm1"
};

struct Bloom
{
	const char *texture;
	float scale;
};

static const Bloom s_bloomWhitelist[] =
{
	{ "models/mapobjects/baph/bapholamp_fx*", 1.0f },
	{ "models/mapobjects/baph/wrist.*", 1.0f },
	{ "models/mapobjects/barrel/barrel2fx.*", 1.0f },
	{ "models/mapobjects/bitch/forearm0*", 1.0f },
	{ "models/mapobjects/bitch/hologirl2.*", 1.0f },
	{ "models/mapobjects/bitch/orb.*", 1.0f },
	{ "models/mapobjects/barrel/colua0/colua0_flare.*", 1.0 },
	{ "models/mapobjects/flares/*", 1.0 },
	{ "models/mapobjects/gratelamp/gratelamp_flare.*", 1.0 },
	{ "models/mapobjects/jets/jet_1.*", 1.0 },
	{ "models/mapobjects/jets/jet_as.*", 1.0 },
	{ "models/mapobjects/lamps/bot_flare*", 1.0 },
	{ "models/mapobjects/lamps/flare03.*", 1.0 },
	{ "models/mapobjects/podium/podiumfx1b.*", 1.0 },
	{ "models/mapobjects/podium/podskullfx.*", 1.0 },
	{ "models/mapobjects/portal_2/portal_3_edge_glo.*", 1.0f },
	{ "models/mapobjects/portal_2/portal_3_glo.*", 1.0f },
	{ "models/mapobjects/slamp/slamp3.*", 1.0f },
	{ "models/mapobjects/spotlamp/spotlamp_l.*", 1.0f },
	{ "models/players/doom/phobos_fx.*", 1.0f },
	{ "models/players/slash/slashskate.*", 1.0f },
	{ "models/weapons2/plasma/plasma_glo.*", 1.0f },
	{ "models/weapons2/railgun/railgun2.glow.*", 1.0f },
	//{ "models/mapobjects/teleporter/energy*", 1.0f },
	{ "models/mapobjects/teleporter/teleporter_edge2.*", 1.0f },
	{ "models/mapobjects/teleporter/transparency2.*", 1.0f },
	{ "textures/base_floor/tilefloor7_owfx.*", 1.0f },
	{ "textures/base_light/baslt4_1.blend.*", 1.0f },
	{ "textures/base_light/border7_ceil50glow.*", 1.0f },
	{ "textures/base_light/border11light.blend.*", 1.0f },
	{ "textures/base_light/ceil1_22a.blend.*", 1.0f },
	{ "textures/base_light/ceil1_30.blend.*", 1.0f },
	{ "textures/base_light/ceil1_34.blend.*", 1.0f },
	{ "textures/base_light/ceil1_37.blend.*", 1.0f },
	{ "textures/base_light/ceil1_4.blend.*", 1.0f },
	{ "textures/base_light/ceil1_38.blend.*", 1.0f },
	{ "textures/base_light/ceil1_39.blend.*", 1.0f },
	{ "textures/base_light/cornerlight.glow.*", 1.0f },
	{ "textures/base_light/geolight_glow.*", 1.0f },
	{ "textures/base_light/jaildr1_3.blend.*", 1.0f },
	{ "textures/base_light/jaildr03_2.blend.*", 1.0f },
	{ "textures/base_light/light1.blend.*", 1.0f },
	{ "textures/base_light/light1blue.blend.*", 1.0f },
	{ "textures/base_light/light1red.blend.*", 1.0f },
	{ "textures/base_light/light2.blend.*", 1.0f },
	{ "textures/base_light/patch10_pj_lite.blend.*", 1.0f },
	{ "textures/base_light/patch10_pj_lite2.blend.*", 1.0f },
	{ "textures/base_light/proto_light2.*", 1.0f },
	{ "textures/base_light/proto_lightred.*", 1.0f },
	{ "textures/base_light/runway_glow.*", 1.0f },
	{ "textures/base_light/trianglelight.blend.*", 1.0f },
	{ "textures/base_light/scrolllight2.*", 1.0f },
	{ "textures/base_light/wsupprt1_12.blend.*", 1.0f },
	{ "textures/base_light/xlight5.blend.*", 1.0f },
	{ "textures/base_trim/border11c_light.*", 1.0f },
	{ "textures/base_trim/border11c_pulse1b.*", 1.0f },
	{ "textures/base_trim/border11light.glow.*", 1.0f },
	{ "textures/base_trim/spiderbit_fx.*", 1.0f },
	{ "textures/base_trim/techborder_fx.*", 1.0f },
	{ "textures/base_wall/basewall01_owfx.*", 1.0f },
	{ "textures/base_wall/basewall01bitfx.*", 1.0f },
	{ "textures/base_wall/bluemetalsupport2bglow.*", 1.0f },
	{ "textures/base_wall/bluemetalsupport2clight.glow.*", 1.0f },
	{ "textures/base_wall/bluemetalsupport2eyel.glow.*", 1.0f },
	{ "textures/base_wall/bluemetalsupport2fline_glow.*", 1.0f },
	{ "textures/base_wall/bluemetalsupport2ftv_glow.*", 1.0f },
	{ "textures/base_wall/dooreyelight.*", 1.0f },
	//{ "textures/ctf/blue_telep*", 1.0f },
	//{ "textures/ctf/red_telep*", 1.0f },
	{ "textures/gothic_block/evil2cglow.*", 1.0f },
	{ "textures/gothic_block/evil2ckillblockglow.*", 1.0f },
	{ "textures/gothic_block/killblock_i4glow.*", 1.0f },
	{ "textures/gothic_light/border7_ceil39b.blend.*", 1.0f },
	{ "textures/gothic_light/border_ceil39.blend.*", 1.0f },
	{ "textures/gothic_light/gothic_light2_blend.*", 1.0f },
	{ "textures/gothic_light/ironcrosslt2.blend.*", 1.0f },
	{ "textures/gothic_light/pentagram_light1_blend.*", 1.0f },
	{ "textures/effects/envmapbfg.*", 1.0f },
	{ "textures/liquids/bubbles.*", 3.0f },
	{ "textures/liquids/lavahell.*", 3.0f },
	{ "textures/liquids/protolava*", 3.0f },
	{ "textures/liquids/slime7*", 3.0f },
	{ "textures/sfx/compscreen/*", 1.0f },
	{ "textures/sfx/b_flame*", 1.0f },
	{ "textures/sfx/bouncepad01b_layer1.*", 1.0f },
	{ "textures/sfx/demonltblackfinal_glow2.*", 1.0f },
	{ "textures/sfx/electric2.blend.*", 1.0f },
	{ "textures/sfx/electricgrade3.*", 1.0f },
	{ "textures/sfx/electricslime*", 1.0f },
	{ "textures/sfx/fire_ctf*", 1.0f },
	{ "textures/sfx/firegorre.*", 1.0f },
	{ "textures/sfx/firegorre2.*", 1.0f },
	{ "textures/sfx/fireswirl2.*", 1.0f },
	{ "textures/sfx/fireswirl2blue.*", 1.0f },
	{ "textures/sfx/firewalla.*", 1.0f },
	{ "textures/sfx/flame*", 1.0f },
	{ "textures/sfx/g_flame*", 1.0f },
	{ "textures/sfx/hologirl.*", 1.0f },
	{ "textures/sfx/jacobs_x.*", 1.0f },
	{ "textures/sfx/jumppadsmall.*", 1.0f },
	{ "textures/sfx/launchpad_arrow.*", 1.0f },
	{ "textures/sfx/launchpad_dot.*", 1.0f },
	{ "textures/sfx/metalfloor_wall_5bglowblu.*", 1.0f },
	{ "textures/sfx/metalfloor_wall_9bglow.*", 1.0f },
	{ "textures/sfx/metalfloor_wall_14bglow2.*", 1.0f },
	{ "textures/sfx/pentagramfloor_red_glow.*", 1.0f },
	{ "textures/sfx/portal_sfx1.*", 1.0f }, // Fixes bloom through textures/sfx/portal_sfx portal effect.
	{ "textures/sfx/portal_sfx_ring_electric.*", 1.0f },
	{ "textures/sfx/proto_zzzt*", 1.0f },
	{ "textures/sfx/r_flame*", 1.0f },
	{ "textures/sfx/surface6jumppad.blend.*", 1.0f },
	{ "textures/sfx/tesla1.*", 1.0f },
	{ "textures/sfx/tesla1b.*", 1.0f },
	{ "textures/sfx/x_conduit2.*", 1.0f },
	{ "textures/sfx/x_conduit3.*", 1.0f },
	{ "textures/sfx/xian_dm3padwallglow.*", 1.0f },
	{ "textures/sfx/zap_scroll.*", 1.0f },
	{ "textures/sfx/zap_scroll2.*", 1.0f },
	{ "textures/skies/*", 1.0f }
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
	if (!isWorldScene)
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
				const Bloom &bloom = s_bloomWhitelist[k];
				const char *wildcard = strstr(bloom.texture, "*");

				if (!util::Stricmp(texture->getName(), bloom.texture) || (wildcard && !util::Stricmpn(texture->getName(), bloom.texture, wildcard - bloom.texture)))
				{
					stage.bloom = true;
					stage.bloomScale = bloom.scale;
				}
			}
		}
	}

	if (g_cvars.lerpTextureAnimation.getBool())
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

	if (g_cvars.waterReflections.getBool())
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
			stage->bundles[0].textures[0] = Texture::find("*reflection");
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
