$input a_position, a_normal, a_tangent, a_texcoord0, a_color0
$output v_position, v_projPosition, v_shadowPosition, v_texcoord0, v_texcoord1, v_normal, v_color0

/*
===========================================================================
Copyright (C) 2010 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <bgfx_shader.sh>
#include "Common.sh"
#include "Gen_Deform.sh"
#include "Gen_Tex.sh"
#include "SharedDefines.sh"
#include "SunLight.sh"

uniform vec4 u_DepthRangeEnabled; // only x used
uniform vec4 u_DepthRange;
uniform vec4 u_BaseColor;
uniform vec4 u_VertColor;
uniform vec4 u_LightType; // only x used
uniform vec4 u_ViewOrigin;
uniform vec4 u_ViewUp;
uniform vec4 u_LocalViewOrigin;
uniform vec4 u_Time; // only x used

uniform vec4 u_Generators;
#define u_TCGen0 int(u_Generators[GEN_TEXCOORD])
#define u_ColorGen int(u_Generators[GEN_COLOR])
#define u_AlphaGen int(u_Generators[GEN_ALPHA])

// tcgen
uniform vec4 u_TCGen0Vector0;
uniform vec4 u_TCGen0Vector1;

// tcmod
uniform vec4 u_DiffuseTexMatrix;
uniform vec4 u_DiffuseTexOffTurb;

// colorgen and alphagen
uniform vec4 u_PortalRange;

uniform vec4 u_FogEnabled; // only x used
uniform vec4 u_FogColorMask;
uniform vec4 u_FogDepth;
uniform vec4 u_FogDistance;
uniform vec4 u_FogEyeT; // only x used

vec2 GenTexCoords(vec3 position, vec3 normal, vec2 texCoord1, vec2 texCoord2)
{
	vec2 tex = texCoord1;

	if (u_TCGen0 == TCGEN_LIGHTMAP)
	{
		tex = texCoord2;
	}
	else if (u_TCGen0 == TCGEN_ENVIRONMENT_MAPPED)
	{
		vec3 viewer = normalize(u_LocalViewOrigin.xyz - position);
		vec2 ref = reflect(viewer, normal).yz;
		tex.x = ref.x * -0.5 + 0.5;
		tex.y = ref.y *  0.5 + 0.5;
	}
	else if (u_TCGen0 == TCGEN_VECTOR)
	{
		tex = vec2(dot(position, u_TCGen0Vector0.xyz), dot(position, u_TCGen0Vector1.xyz));
	}
	
	return tex;
}

vec4 CalcColor(vec4 vertColor, vec4 baseColor, vec4 colorAttrib, vec3 position, vec3 normal)
{
	vec4 color = vertColor * colorAttrib + baseColor;
	vec3 viewer = u_LocalViewOrigin.xyz - position;

	if (u_AlphaGen == AGEN_LIGHTING_SPECULAR)
	{
		vec3 lightDir = normalize(vec3(-960.0, 1980.0, 96.0) - position);
		vec3 reflected = -reflect(lightDir, normal);
		
		color.a = saturate(dot(reflected, normalize(viewer)));
		color.a *= color.a;
		color.a *= color.a;
	}
	else if (u_AlphaGen == AGEN_PORTAL)
	{
		color.a = saturate(length(viewer) / u_PortalRange.x);
	}
	
	return color;
}

void main()
{
	vec3 position = a_position;
	vec3 normal = a_normal;

	if (int(u_NumDeforms.x) > 0)
	{
		CalculateDeform(position, normal, a_texcoord0.xy, u_Time.x);
	}

	if (u_TCGen0 != TCGEN_NONE)
	{
		vec2 tex = GenTexCoords(position, normal, a_texcoord0.xy, a_texcoord0.zw);
		v_texcoord0 = ModTexCoords(tex, position, u_DiffuseTexMatrix, u_DiffuseTexOffTurb);
	}
	else
	{
		v_texcoord0 = a_texcoord0.xy;
	}

	if ((u_ColorGen != CGEN_IDENTITY || u_AlphaGen != AGEN_IDENTITY) && int(u_LightType.x) == LIGHT_NONE)
	{
		v_color0 = CalcColor(u_VertColor, u_BaseColor, a_color0, position, normal);
	}
	else
	{
		v_color0 = u_VertColor * a_color0 + u_BaseColor;
	}

	if (int(u_FogEnabled.x) != 0)
	{
		v_color0 *= vec4_splat(1.0) - u_FogColorMask * sqrt(saturate(CalcFog(position, u_FogDepth, u_FogDistance, u_FogEyeT.x)));
	}

	vec3 wsPosition = mul(u_model[0], vec4(position, 1.0)).xyz;
	v_texcoord1 = a_texcoord0.zw;
	v_position = wsPosition;
	v_normal = mul(u_model[0], vec4(normal, 0.0));
	v_projPosition = mul(u_viewProj, vec4(v_position, 1.0));
	if (int(u_DepthRangeEnabled.x) != 0)
		v_projPosition = ApplyDepthRange(v_projPosition, u_DepthRange.x, u_DepthRange.y);
#if defined(USE_SUN_LIGHT)
	v_shadowPosition = mul(u_LightModelViewProj, vec4(mul(u_model[0], vec4(a_position, 1.0)).xyz + v_normal.xyz * u_ShadowMapNormalBias, 1.0));
#endif
	gl_Position = v_projPosition;
}
