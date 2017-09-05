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
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of Daemon source code.

Daemon source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Daemon source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "Precompiled.h"
#pragma hdrstop

#define	WAVEVALUE(table, base, amplitude, phase, freq) ((base) + table[std::lrintf((((phase) + material->time_ * (freq)) * g_funcTableSize)) & g_funcTableMask] * (amplitude))

namespace renderer {

vec4 MaterialStage::getFogColorMask() const
{
	assert(active);

	switch(adjustColorsForFog)
	{
	case MaterialAdjustColorsForFog::ModulateRGB:
		return vec4(1, 1, 1, 0);
	case MaterialAdjustColorsForFog::ModulateAlpha:
		return vec4(0, 0, 0, 1);
	case MaterialAdjustColorsForFog::ModulateRGBA:
		return vec4(1, 1, 1, 1);
	default:
		break;
	}

	return vec4(0, 0, 0, 0);
}

uint64_t MaterialStage::getState() const
{
	assert(active);
	uint64_t state = BGFX_STATE_BLEND_FUNC(blendSrc, blendDst);
	state |= depthTestBits;

	if (depthWrite)
	{
		state |= BGFX_STATE_DEPTH_WRITE;
	}

	if (material->cullType != MaterialCullType::TwoSided)
	{
		bool cullBack = (material->cullType == MaterialCullType::FrontSided);

		if (main::IsCameraMirrored())
			cullBack = !cullBack;

		state |= cullBack ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW;
	}

	return state;
}

void MaterialStage::setShaderUniforms(Uniforms_MaterialStage *uniforms, int flags) const
{
	uniforms->alphaTest.set((float)alphaTest);

	if (shouldLerpTextureAnimation())
	{
		float fraction;
		calculateTextureAnimation(nullptr, nullptr, &fraction);
		uniforms->animation_Enabled_Fraction.set(vec4(1, fraction, 0, 0));
	}
	else
	{
		uniforms->animation_Enabled_Fraction.set(vec4::empty);
	}

	uniforms->lightType.set(vec4((float)light, 0, 0, 0));
	uniforms->normalScale.set(normalScale);
	uniforms->specularScale.set(specularScale);

	if (flags & (MaterialStageSetUniformsFlags::ColorGen | MaterialStageSetUniformsFlags::TexGen))
	{
		vec4 generators;
		generators[Uniforms_MaterialStage::Generators::TexCoord] = (float)bundles[0].tcGen;
		generators[Uniforms_MaterialStage::Generators::Color] = (float)rgbGen;
		generators[Uniforms_MaterialStage::Generators::Alpha] = (float)alphaGen;
		uniforms->generators.set(generators);
	}

	if (flags & MaterialStageSetUniformsFlags::ColorGen)
	{
		// rgbGen and alphaGen
		vec4 baseColor, vertexColor;
		calculateColors(&baseColor, &vertexColor);
		uniforms->baseColor.set(util::ToLinear(baseColor));
		uniforms->vertexColor.set(util::ToLinear(vertexColor));

		if (alphaGen == MaterialAlphaGen::Portal)
		{
			uniforms->portalRange.set(material->portalRange);
		}
	}

	if (flags & MaterialStageSetUniformsFlags::TexGen)
	{
		// tcGen and tcMod
		vec4 texMatrix, texOffTurb;
		calculateTexMods(&texMatrix, &texOffTurb);
		uniforms->diffuseTextureMatrix.set(texMatrix);
		uniforms->diffuseTextureOffsetTurbulent.set(texOffTurb);

		if (bundles[0].tcGen == MaterialTexCoordGen::Vector)
		{
			uniforms->tcGenVector0.set(bundles[0].tcGenVectors[0]);
			uniforms->tcGenVector1.set(bundles[0].tcGenVectors[1]);
		}
	}
}

void MaterialStage::setTextureSamplers(Uniforms_MaterialStage *uniforms) const
{
	assert(uniforms);
	assert(active);

	// Diffuse.
	const MaterialTextureBundle &diffuseBundle = bundles[MaterialTextureBundleIndex::DiffuseMap];

	if (diffuseBundle.isVideoMap)
	{
		interface::CIN_RunCinematic(diffuseBundle.videoMapHandle);
		interface::CIN_UploadCinematic(diffuseBundle.videoMapHandle);
	}

	if (diffuseBundle.numImageAnimations <= 1)
	{
		bgfx::setTexture(TextureUnit::Diffuse, uniforms->diffuseSampler.handle, diffuseBundle.textures[0]->getHandle());

#ifdef _DEBUG
		bgfx::setTexture(TextureUnit::Diffuse2, uniforms->diffuseSampler2.handle, g_textureCache->getWhite()->getHandle());
#endif
	}
	else
	{
		int frame, nextFrame;
		calculateTextureAnimation(&frame, &nextFrame, nullptr);
		bgfx::setTexture(TextureUnit::Diffuse, uniforms->diffuseSampler.handle, diffuseBundle.textures[frame]->getHandle());

		if (shouldLerpTextureAnimation())
		{
			bgfx::setTexture(TextureUnit::Diffuse2, uniforms->diffuseSampler2.handle, diffuseBundle.textures[nextFrame]->getHandle());
		}
#ifdef _DEBUG
		else
		{
			bgfx::setTexture(TextureUnit::Diffuse2, uniforms->diffuseSampler2.handle, g_textureCache->getWhite()->getHandle());
		}
#endif
	}

	// Lightmap.
	const Texture *lightmap = bundles[MaterialTextureBundleIndex::Lightmap].textures[0];

	if (lightmap)
	{
		bgfx::setTexture(TextureUnit::Light, uniforms->lightSampler.handle, lightmap->getHandle());
	}
#ifdef _DEBUG
	else
	{
		bgfx::setTexture(TextureUnit::Light, uniforms->lightSampler.handle, g_textureCache->getWhite()->getHandle());
	}
#endif
}

bool MaterialStage::shouldLerpTextureAnimation() const
{
	return bundles[MaterialTextureBundleIndex::DiffuseMap].numImageAnimations > 1 && textureAnimationLerp != MaterialStageTextureAnimationLerp::Disabled && main::IsLerpTextureAnimationEnabled();
}

void MaterialStage::calculateTextureAnimation(int *frame, int *nextFrame, float *fraction) const
{
	const MaterialTextureBundle &diffuseBundle = bundles[MaterialTextureBundleIndex::DiffuseMap];

	if (frame)
	{
		// It is necessary to do this messy calc to make sure animations line up exactly with waveforms of the same frequency.
		*frame = std::lrintf(material->time_ * diffuseBundle.imageAnimationSpeed * g_funcTableSize);
		*frame >>= g_funcTableSize2;
		*frame = std::max(0, *frame); // May happen with shader time offsets.
		*frame %= diffuseBundle.numImageAnimations;
	}

	if (nextFrame)
	{
		assert(frame);
		*nextFrame = *frame + 1;

		if (textureAnimationLerp == MaterialStageTextureAnimationLerp::Clamp)
		{
			*nextFrame = std::min(*nextFrame, diffuseBundle.numImageAnimations - 1);
		}
		else
		{
			*nextFrame %= diffuseBundle.numImageAnimations;
		}
	}

	if (fraction)
	{
		float temp;
		*fraction = std::modf(material->time_ * diffuseBundle.imageAnimationSpeed, &temp);
	}
}

float *MaterialStage::tableForFunc(MaterialWaveformGenFunc func) const
{
	switch(func)
	{
	case MaterialWaveformGenFunc::Sin:
		return g_sinTable;
	case MaterialWaveformGenFunc::Triangle:
		return g_triangleTable;
	case MaterialWaveformGenFunc::Square:
		return g_squareTable;
	case MaterialWaveformGenFunc::Sawtooth:
		return g_sawToothTable;
	case MaterialWaveformGenFunc::InverseSawtooth:
		return g_inverseSawToothTable;
	case MaterialWaveformGenFunc::None:
	default:
		break;
	}

	interface::Error("TableForFunc called with invalid function '%d' in shader '%s'", func, material->name);
	return NULL;
}

float MaterialStage::evaluateWaveForm(const MaterialWaveForm &wf) const
{
	return WAVEVALUE(tableForFunc(wf.func), wf.base, wf.amplitude, wf.phase, wf.frequency);
}

float MaterialStage::evaluateWaveFormClamped(const MaterialWaveForm &wf) const
{
	return math::Clamped(evaluateWaveForm(wf), 0.0f, 1.0f);
}

void MaterialStage::calculateTexMods(vec4 *outMatrix, vec4 *outOffTurb) const
{
	assert(outMatrix);
	assert(outOffTurb);

	float matrix[6] = { 1, 0, 0, 1, 0, 0 };
	float currentMatrix[6] = { 1, 0, 0, 1, 0, 0 };
	(*outMatrix) = { 1, 0, 0, 1 };
	(*outOffTurb) = { 0, 0, 0, 0 };
	const MaterialTextureBundle &bundle = bundles[0];

	for (int tm = 0; tm < bundle.numTexMods; tm++)
	{
		switch (bundle.texMods[tm].type)
		{
		case MaterialTexMod::None:
			tm = MaterialTextureBundle::maxTexMods; // break out of for loop
			break;

		case MaterialTexMod::Turbulent:
			calculateTurbulentFactors(bundle.texMods[tm].wave, &(*outOffTurb)[2], &(*outOffTurb)[3]);
			break;

		case MaterialTexMod::EntityTranslate:
			calculateScrollTexMatrix(main::GetCurrentEntity()->materialTexCoord, matrix);
			break;

		case MaterialTexMod::Scroll:
			calculateScrollTexMatrix(bundle.texMods[tm].scroll, matrix);
			break;

		case MaterialTexMod::Scale:
			calculateScaleTexMatrix(bundle.texMods[tm].scale, matrix);
			break;
		
		case MaterialTexMod::Stretch:
			calculateStretchTexMatrix(bundle.texMods[tm].wave,  matrix);
			break;

		case MaterialTexMod::Transform:
			calculateTransformTexMatrix(bundle.texMods[tm], matrix);
			break;

		case MaterialTexMod::Rotate:
			calculateRotateTexMatrix(bundle.texMods[tm].rotateSpeed, matrix);
			break;

		default:
			interface::Error("ERROR: unknown texmod '%d' in shader '%s'", bundle.texMods[tm].type, material->name);
			break;
		}

		switch (bundle.texMods[tm].type)
		{	
		case MaterialTexMod::None:
		case MaterialTexMod::Turbulent:
		default:
			break;

		case MaterialTexMod::EntityTranslate:
		case MaterialTexMod::Scroll:
		case MaterialTexMod::Scale:
		case MaterialTexMod::Stretch:
		case MaterialTexMod::Transform:
		case MaterialTexMod::Rotate:
			(*outMatrix)[0] = matrix[0] * currentMatrix[0] + matrix[2] * currentMatrix[1];
			(*outMatrix)[1] = matrix[1] * currentMatrix[0] + matrix[3] * currentMatrix[1];
			(*outMatrix)[2] = matrix[0] * currentMatrix[2] + matrix[2] * currentMatrix[3];
			(*outMatrix)[3] = matrix[1] * currentMatrix[2] + matrix[3] * currentMatrix[3];

			(*outOffTurb)[0] = matrix[0] * currentMatrix[4] + matrix[2] * currentMatrix[5] + matrix[4];
			(*outOffTurb)[1] = matrix[1] * currentMatrix[4] + matrix[3] * currentMatrix[5] + matrix[5];

			currentMatrix[0] = (*outMatrix)[0];
			currentMatrix[1] = (*outMatrix)[1];
			currentMatrix[2] = (*outMatrix)[2];
			currentMatrix[3] = (*outMatrix)[3];
			currentMatrix[4] = (*outOffTurb)[0];
			currentMatrix[5] = (*outOffTurb)[1];
			break;
		}
	}
}

void MaterialStage::calculateTurbulentFactors(const MaterialWaveForm &wf, float *amplitude, float *now) const
{
	*now = wf.phase + material->time_ * wf.frequency;
	*amplitude = wf.amplitude;
}

void MaterialStage::calculateScaleTexMatrix(vec2 scale, float *matrix) const
{
	matrix[0] = scale[0]; matrix[2] = 0.0f;     matrix[4] = 0.0f;
	matrix[1] = 0.0f;     matrix[3] = scale[1]; matrix[5] = 0.0f;
}

void MaterialStage::calculateScrollTexMatrix(vec2 scrollSpeed, float *matrix) const
{
	float adjustedScrollS = scrollSpeed[0] * material->time_;
	float adjustedScrollT = scrollSpeed[1] * material->time_;

	// clamp so coordinates don't continuously get larger, causing problems with hardware limits
	adjustedScrollS = adjustedScrollS - floor(adjustedScrollS);
	adjustedScrollT = adjustedScrollT - floor(adjustedScrollT);

	matrix[0] = 1.0f; matrix[2] = 0.0f; matrix[4] = adjustedScrollS;
	matrix[1] = 0.0f; matrix[3] = 1.0f; matrix[5] = adjustedScrollT;
}

void MaterialStage::calculateStretchTexMatrix(const MaterialWaveForm &wf, float *matrix) const
{
	const float p = 1.0f / evaluateWaveForm(wf);
	matrix[0] = p; matrix[2] = 0; matrix[4] = 0.5f - 0.5f * p;
	matrix[1] = 0; matrix[3] = p; matrix[5] = 0.5f - 0.5f * p;
}

void MaterialStage::calculateTransformTexMatrix(const MaterialTexModInfo &tmi, float *matrix) const
{
	matrix[0] = tmi.matrix[0][0]; matrix[2] = tmi.matrix[1][0]; matrix[4] = tmi.translate[0];
	matrix[1] = tmi.matrix[0][1]; matrix[3] = tmi.matrix[1][1]; matrix[5] = tmi.translate[1];
}

void MaterialStage::calculateRotateTexMatrix(float degsPerSecond, float *matrix) const
{
	float degs = -degsPerSecond * material->time_;
	int index = int(degs * (g_funcTableSize / 360.0f));
	float sinValue = g_sinTable[index & g_funcTableMask];
	float cosValue = g_sinTable[(index + g_funcTableSize / 4) & g_funcTableMask];
	matrix[0] = cosValue; matrix[2] = -sinValue; matrix[4] = 0.5f - 0.5f * cosValue + 0.5f * sinValue;
	matrix[1] = sinValue; matrix[3] = cosValue;  matrix[5] = 0.5f - 0.5f * sinValue - 0.5f * cosValue;
}

float MaterialStage::calculateWaveColorSingle(const MaterialWaveForm &wf) const
{
	float glow;

	if (wf.func == MaterialWaveformGenFunc::Noise)
	{
		glow = wf.base + main::CalculateNoise(0, 0, 0, (material->time_ + wf.phase ) * wf.frequency ) * wf.amplitude;
	}
	else
	{
		glow = evaluateWaveForm(wf) * g_identityLight;
	}
	
	return math::Clamped(glow, 0.0f, 1.0f);
}

float MaterialStage::calculateWaveAlphaSingle(const MaterialWaveForm &wf) const
{
	return evaluateWaveFormClamped(wf);
}

void MaterialStage::calculateColors(vec4 *baseColor, vec4 *vertColor) const
{
	assert(baseColor);
	assert(vertColor);

	*baseColor = vec4::white;
	*vertColor = vec4(0, 0, 0, 0);

	// rgbGen
	switch (rgbGen)
	{
		case MaterialColorGen::IdentityLighting:
			(*baseColor).r = (*baseColor).g = (*baseColor).b = g_identityLight;
			break;
		case MaterialColorGen::ExactVertex:
		case MaterialColorGen::ExactVertexLit:
			*baseColor = vec4::black;
			*vertColor = vec4::white;
			break;
		case MaterialColorGen::Const:
			(*baseColor) = constantColor;
			break;
		case MaterialColorGen::Vertex:
			*baseColor = vec4::black;
			*vertColor = vec4(g_identityLight, g_identityLight, g_identityLight, 1);
			break;
		case MaterialColorGen::VertexLit:
			*baseColor = vec4::black;
			*vertColor = vec4(g_identityLight);
			break;
		case MaterialColorGen::OneMinusVertex:
			(*baseColor).r = (*baseColor).g = (*baseColor).b = g_identityLight;
			(*vertColor).r = (*vertColor).g = (*vertColor).b = -g_identityLight;
			break;
		case MaterialColorGen::Fog:
			/*{
				fog_t		*fog;

				fog = tr.world->fogs + tess.fogNum;

				(*baseColor).r = ((unsigned char *)(&fog->colorInt)).r / 255.0f;
				(*baseColor).g = ((unsigned char *)(&fog->colorInt)).g / 255.0f;
				(*baseColor).b = ((unsigned char *)(&fog->colorInt)).b / 255.0f;
				(*baseColor).a = ((unsigned char *)(&fog->colorInt)).a / 255.0f;
			}*/
			break;
		case MaterialColorGen::Waveform:
			(*baseColor).r = (*baseColor).g = (*baseColor).b = calculateWaveColorSingle(rgbWave);
			break;
		case MaterialColorGen::Entity:
			if (main::GetCurrentEntity())
			{
				(*baseColor) = main::GetCurrentEntity()->materialColor;
			}
			break;
		case MaterialColorGen::OneMinusEntity:
			if (main::GetCurrentEntity())
			{
				(*baseColor).r = 1.0f - main::GetCurrentEntity()->materialColor.r;
				(*baseColor).g = 1.0f - main::GetCurrentEntity()->materialColor.g;
				(*baseColor).b = 1.0f - main::GetCurrentEntity()->materialColor.b;
				(*baseColor).a = 1.0f - main::GetCurrentEntity()->materialColor.a;
			}
			break;
		case MaterialColorGen::Identity:
		case MaterialColorGen::LightingDiffuse:
		case MaterialColorGen::Bad:
			break;
	}

	// alphaGen
	switch (alphaGen)
	{
		case MaterialAlphaGen::Skip:
			break;
		case MaterialAlphaGen::Const:
			(*baseColor).a = constantColor.a;
			(*vertColor).a = 0.0f;
			break;
		case MaterialAlphaGen::Waveform:
			(*baseColor).a = calculateWaveAlphaSingle(alphaWave);
			(*vertColor).a = 0.0f;
			break;
		case MaterialAlphaGen::Entity:
			if (main::GetCurrentEntity())
			{
				(*baseColor).a = main::GetCurrentEntity()->materialColor.a;
			}
			(*vertColor).a = 0.0f;
			break;
		case MaterialAlphaGen::OneMinusEntity:
			if (main::GetCurrentEntity())
			{
				(*baseColor).a = 1.0f - main::GetCurrentEntity()->materialColor.a;
			}
			(*vertColor).a = 0.0f;
			break;
		case MaterialAlphaGen::NormalZFade:
			// TODO
			break;
		case MaterialAlphaGen::Vertex:
			(*baseColor).a = 0.0f;
			(*vertColor).a = 1.0f;
			break;
		case MaterialAlphaGen::OneMinusVertex:
			(*baseColor).a = 1.0f;
			(*vertColor).a = -1.0f;
			break;
		case MaterialAlphaGen::Identity:
		case MaterialAlphaGen::LightingSpecular:
		case MaterialAlphaGen::Portal:
			// Done entirely in vertex program
			(*baseColor).a = 1.0f;
			(*vertColor).a = 0.0f;
			break;
	}

	// Multiply color by overbright factor.
	// The GL1 renderer does this to texture color at load time instead.
	if (!g_hardwareGammaEnabled && g_overbrightFactor > 1)
	{
		const bool isBlend = (blendSrc == BGFX_STATE_BLEND_DST_COLOR || blendSrc == BGFX_STATE_BLEND_INV_DST_COLOR || blendDst == BGFX_STATE_BLEND_SRC_COLOR || blendDst == BGFX_STATE_BLEND_INV_SRC_COLOR);

		// Hack around materials with lightmap only stages (white diffuse * lightmap) like textures/base_wall/bluemetal1b_shiny (q3dm12).
		// These will have a lightmap only first stage with the second stage doing multiply blend.
		// Normally, the first stage will be multiplied by overbright factor, but not the second stage, resulting in ugly clamping artifacts.
		// Fix this by swapping which stage gets the overbright factor multiply.
		if (material->stages[0].bundles[MaterialTextureBundleIndex::DiffuseMap].textures[0] == g_textureCache->getWhite() && material->stages[0].bundles[MaterialTextureBundleIndex::Lightmap].isLightmap)
		{
			// First stage is lightmap only.
			// If this is the first stage, don't multiply by overbright factor. Otherwise, multiply even if it's a blend.
			if (this == &material->stages[0])
				return;
		}
		else if (isBlend)
		{
			return;
		}
		
		(*baseColor) = vec4(baseColor->xyz() * g_overbrightFactor, baseColor->a);
		(*vertColor) = vec4(vertColor->xyz() * g_overbrightFactor, vertColor->a);
	}
}

float Material::setTime(float time)
{
	time_ = time - timeOffset;

	if (main::GetCurrentEntity())
	{
		time_ -= main::GetCurrentEntity()->materialTime;
	}

	return time_;
}

bool Material::hasAutoSpriteDeform() const
{
	for (const MaterialDeformStage &ds : deforms)
	{
		if (ds.deformation == MaterialDeform::Autosprite || ds.deformation == MaterialDeform::Autosprite2)
			return true;
	}

	return false;
}

void Material::doAutoSpriteDeform(const mat3 &sceneRotation, Vertex *vertices, uint32_t nVertices, uint16_t *indices, uint32_t nIndices, float *softSpriteDepth) const
{
	assert(vertices);
	assert(indices);
	assert(softSpriteDepth);
	MaterialDeform deform = MaterialDeform::None;

	for (const MaterialDeformStage &ds : deforms)
	{
		if (ds.deformation == MaterialDeform::Autosprite || ds.deformation == MaterialDeform::Autosprite2)
		{
			deform = ds.deformation;
			break;
		}
	}

	if (deform == MaterialDeform::None)
	{
		*softSpriteDepth = 0;
		return;
	}

	if ((nIndices % 6) != 0)
	{
		interface::PrintWarningf("Autosprite material %s had odd index count %u\n", name, nIndices);
	}

	vec3 forward, leftDir, upDir;

	if (main::GetCurrentEntity())
	{
		forward.x = vec3::dotProduct(sceneRotation[0], main::GetCurrentEntity()->rotation[0]);
		forward.y = vec3::dotProduct(sceneRotation[0], main::GetCurrentEntity()->rotation[1]);
		forward.z = vec3::dotProduct(sceneRotation[0], main::GetCurrentEntity()->rotation[2]);
		leftDir.x = vec3::dotProduct(sceneRotation[1], main::GetCurrentEntity()->rotation[0]);
		leftDir.y = vec3::dotProduct(sceneRotation[1], main::GetCurrentEntity()->rotation[1]);
		leftDir.z = vec3::dotProduct(sceneRotation[1], main::GetCurrentEntity()->rotation[2]);
		upDir.x = vec3::dotProduct(sceneRotation[2], main::GetCurrentEntity()->rotation[0]);
		upDir.y = vec3::dotProduct(sceneRotation[2], main::GetCurrentEntity()->rotation[1]);
		upDir.z = vec3::dotProduct(sceneRotation[2], main::GetCurrentEntity()->rotation[2]);
	}
	else
	{
		forward = sceneRotation[0];
		leftDir = sceneRotation[1];
		upDir = sceneRotation[2];
	}

	// Assuming the geometry is triangulated quads.
	// Autosprite will rebuild them as forward facing sprites.
	// Autosprite2 will pivot a rectangular quad along the center of its long axis.
	for (size_t quadIndex = 0; quadIndex < nIndices / 6; quadIndex++)
	{
		//const size_t firstIndex = dc->ib.firstIndex + quadIndex * 6;
		const size_t firstIndex = quadIndex * 6;

		// Get the quad corner vertices and their indexes.
		auto v = util::ExtractQuadCorners(vertices, &indices[firstIndex]);
		std::array<uint16_t, 4> vi;

		for (size_t i = 0; i < vi.size(); i++)
			vi[i] = uint16_t(v[i] - vertices);

		// Find the midpoint.
		const vec3 midpoint = (v[0]->pos + v[1]->pos + v[2]->pos + v[3]->pos) * 0.25f;
		const float radius = (v[0]->pos - midpoint).length() * 0.707f; // / sqrt(2)

		// Calculate soft sprite depth.
		*softSpriteDepth = radius / 2;

		if (deform == MaterialDeform::Autosprite)
		{
			vec3 left(leftDir * radius);
			vec3 up(upDir * radius);

			if (main::IsCameraMirrored())
				left = -left;

			// Compensate for scale in the axes if necessary.
			if (main::GetCurrentEntity() && main::GetCurrentEntity()->nonNormalizedAxes)
			{
				float axisLength = vec3(main::GetCurrentEntity()->rotation[0]).length();

				if (!axisLength)
				{
					axisLength = 0;
				}
				else
				{
					axisLength = 1.0f / axisLength;
				}

				left *= axisLength;
				up *= axisLength;
			}

			// Rebuild quad facing the main camera.
			v[0]->pos = midpoint + left + up;
			v[1]->pos = midpoint - left + up;
			v[2]->pos = midpoint - left - up;
			v[3]->pos = midpoint + left - up;

			// Constant normal all the way around.
			v[0]->setNormal(-sceneRotation[0]);
			v[1]->setNormal(-sceneRotation[0]);
			v[2]->setNormal(-sceneRotation[0]);
			v[3]->setNormal(-sceneRotation[0]);

			// Standard square texture coordinates.
			v[0]->setTexCoord(0, 0, 0, 0);
			v[1]->setTexCoord(1, 0, 1, 0);
			v[2]->setTexCoord(1, 1, 1, 1);
			v[3]->setTexCoord(0, 1, 0, 1);

			indices[firstIndex + 0] = vi[0];
			indices[firstIndex + 1] = vi[1];
			indices[firstIndex + 2] = vi[3];
			indices[firstIndex + 3] = vi[3];
			indices[firstIndex + 4] = vi[1];
			indices[firstIndex + 5] = vi[2];
		}
		else if (deform == MaterialDeform::Autosprite2)
		{
			const int edgeVerts[6][2] = { { 0, 1 },{ 0, 2 },{ 0, 3 },{ 1, 2 },{ 1, 3 },{ 2, 3 } };
			uint16_t smallestIndex = indices[firstIndex];

			for (size_t i = 0; i < vi.size(); i++)
			{
				smallestIndex = std::min(smallestIndex, vi[i]);
			}

			// Identify the two shortest edges.
			int nums[2] = {};
			float lengths[2];
			lengths[0] = lengths[1] = 999999;

			for (int i = 0; i < 6; i++)
			{
				const vec3 temp = vec3(v[edgeVerts[i][0]]->pos) - vec3(v[edgeVerts[i][1]]->pos);
				const float l = vec3::dotProduct(temp, temp);

				if (l < lengths[0])
				{
					nums[1] = nums[0];
					lengths[1] = lengths[0];
					nums[0] = i;
					lengths[0] = l;
				}
				else if (l < lengths[1])
				{
					nums[1] = i;
					lengths[1] = l;
				}
			}

			// Find the midpoints.
			vec3 midpoints[2];

			for (int i = 0; i < 2; i++)
			{
				midpoints[i] = (v[edgeVerts[nums[i]][0]]->pos + v[edgeVerts[nums[i]][1]]->pos) * 0.5f;
			}

			// Find the vector of the major axis.
			const vec3 major(midpoints[1] - midpoints[0]);

			// Cross this with the view direction to get minor axis.
			const vec3 minor(vec3::crossProduct(major, forward).normal());

			// Re-project the points.
			for (int i = 0; i < 2; i++)
			{
				// We need to see which direction this edge is used to determine direction of projection.
				int j;

				for (j = 0; j < 5; j++)
				{
					if (indices[firstIndex + j] == smallestIndex + edgeVerts[nums[i]][0] && indices[firstIndex + j + 1] == smallestIndex + edgeVerts[nums[i]][1])
						break;
				}

				const float l = 0.5f * sqrt(lengths[i]);
				vec3 *v1 = &v[edgeVerts[nums[i]][0]]->pos;
				vec3 *v2 = &v[edgeVerts[nums[i]][1]]->pos;

				if (j == 5)
				{
					*v1 = midpoints[i] + minor * l;
					*v2 = midpoints[i] + minor * -l;
				}
				else
				{
					*v1 = midpoints[i] + minor * -l;
					*v2 = midpoints[i] + minor * l;
				}
			}
		}
	}
}

void Material::setDeformUniforms(Uniforms_Material *uniforms) const
{
	assert(uniforms);
	vec4 moveDirs[maxDeforms];
	vec4 gen_Wave_Base_Amplitude[maxDeforms];
	vec4 frequency_Phase_Spread[maxDeforms];
	uint16_t nDeforms = 0;

	for (const MaterialDeformStage &ds : deforms)
	{
		switch (ds.deformation)
		{
		case MaterialDeform::Wave:
			gen_Wave_Base_Amplitude[nDeforms] = vec4((float)ds.deformation, (float)ds.deformationWave.func, ds.deformationWave.base, ds.deformationWave.amplitude);
			frequency_Phase_Spread[nDeforms] = vec4(ds.deformationWave.frequency, ds.deformationWave.phase, ds.deformationSpread, 0);
			nDeforms++;
			break;

		case MaterialDeform::Bulge:
			gen_Wave_Base_Amplitude[nDeforms] = vec4((float)ds.deformation, (float)ds.deformationWave.func, 0, ds.bulgeHeight);
			frequency_Phase_Spread[nDeforms] = vec4(ds.bulgeSpeed, ds.bulgeWidth, 0, 0);
			nDeforms++;
			break;

		case MaterialDeform::Move:
			gen_Wave_Base_Amplitude[nDeforms] = vec4((float)ds.deformation, (float)ds.deformationWave.func, ds.deformationWave.base, ds.deformationWave.amplitude);
			frequency_Phase_Spread[nDeforms] = vec4(ds.deformationWave.frequency, ds.deformationWave.phase, 0, 0);
			moveDirs[nDeforms] = ds.moveVector;
			nDeforms++;
			break;

		default:
			break;
		}
	}

	uniforms->nDeforms.set(vec4(nDeforms, 0, 0, 0));

	if (nDeforms > 0)
	{
		uniforms->deformMoveDirs.set(moveDirs, nDeforms);
		uniforms->deform_Gen_Wave_Base_Amplitude.set(gen_Wave_Base_Amplitude, nDeforms);
		uniforms->deform_Frequency_Phase_Spread.set(frequency_Phase_Spread, nDeforms);
	}
}

} // namespace renderer
