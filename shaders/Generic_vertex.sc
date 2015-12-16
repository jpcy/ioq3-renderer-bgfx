$input a_position, a_normal, a_tangent, a_texcoord0, a_texcoord1, a_texcoord2, a_color0
$output v_position, v_texcoord0, v_texcoord1, v_normal, v_color0, v_viewDir

#include <bgfx_shader.sh>
#include "Common.sh"
#include "Defines.sh"
#include "Generators.sh"

uniform vec4 u_DepthRange;
uniform vec4 u_BaseColor;
uniform vec4 u_VertColor;
uniform vec4 u_LightType; // only x used
uniform vec4 u_ViewOrigin;
uniform vec4 u_Time; // only x used

uniform vec4 u_FogEnabled;
uniform vec4 u_FogDistance;
uniform vec4 u_FogDepth;
uniform vec4 u_FogEyeT;
uniform vec4 u_FogColorMask;

float CalcFog(vec3 position)
{
	float s = dot(vec4(position, 1.0), u_FogDistance) * 8.0;
	float t = dot(vec4(position, 1.0), u_FogDepth);

	float eyeOutside = float(u_FogEyeT.x < 0.0);
	float fogged = float(t >= eyeOutside);

	t += 1e-6;
	t *= fogged / (t - u_FogEyeT.x * eyeOutside);

	return s * t;
}

void main()
{
	vec3 position = a_position;
	vec3 normal = a_normal;

	if (u_NumDeforms.x > 0)
	{
		CalculateDeform(position, normal, a_texcoord0, u_Time.x);
	}

	if (u_TCGen0 != TCGEN_NONE)
	{
		vec2 tex = GenTexCoords(position, normal, a_texcoord0, a_texcoord1);
		v_texcoord0 = ModTexCoords(tex, position, u_DiffuseTexMatrix, u_DiffuseTexOffTurb);
	}
	else
	{
		v_texcoord0 = a_texcoord0;
	}

	if ((u_ColorGen != CGEN_IDENTITY || u_AlphaGen != AGEN_IDENTITY) && u_LightType.x == LIGHT_NONE)
	{
		v_color0 = CalcColor(u_VertColor, u_BaseColor, a_color0, position, normal);
	}
	else
	{
		v_color0 = u_VertColor * a_color0 + u_BaseColor;
	}

	if (u_FogEnabled.x != 0)
	{
		v_color0 *= vec4_splat(1.0) - u_FogColorMask * sqrt(saturate(CalcFog(position)));
	}

	v_texcoord1 = a_texcoord1;
	v_position = mul(u_model[0], vec4(position, 1.0)).xyz;
	v_normal = mul(u_model[0], vec4(normal, 0.0));
	v_viewDir = u_ViewOrigin.xyz - v_position;
	gl_Position = ApplyDepthRange(mul(u_viewProj, vec4(v_position, 1.0)), u_DepthRange.x, u_DepthRange.y);
}
