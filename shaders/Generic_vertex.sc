$input a_position, a_normal, a_texcoord0, a_texcoord1, a_color0
$output v_position, v_texcoord0, v_color0

#include <bgfx_shader.sh>
#include "Common.sh"
#include "Generators.sh"

uniform vec4 u_DepthRange;
uniform vec4 u_BaseColor;
uniform vec4 u_VertColor;

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
	v_position = mul(u_model[0], vec4(a_position, 1.0)).xyz;
	vec3 normal = a_normal;

	if (u_DeformGen != DGEN_NONE)
	{
		v_position = DeformPosition(v_position, normal, a_texcoord0);
	}

	if (u_TCGen0 != TCGEN_NONE)
	{
		vec2 tex = GenTexCoords(v_position, normal, a_texcoord0, a_texcoord1);
		v_texcoord0 = ModTexCoords(tex, v_position, u_DiffuseTexMatrix, u_DiffuseTexOffTurb);
	}
	else
	{
		v_texcoord0 = a_texcoord0;
	}

	if (u_ColorGen != CGEN_IDENTITY || u_AlphaGen != AGEN_IDENTITY)
	{
		v_color0 = CalcColor(u_VertColor, u_BaseColor, a_color0, v_position, normal);
	}
	else
	{
		v_color0 = u_VertColor * a_color0 + u_BaseColor;
	}

	if (u_FogEnabled.x != 0)
	{
		v_color0 *= vec4(1.0, 1.0, 1.0, 1.0) - u_FogColorMask * sqrt(clamp(CalcFog(v_position), 0.0, 1.0));
	}

	gl_Position = ApplyDepthRange(mul(u_viewProj, vec4(v_position, 1.0)), u_DepthRange.x, u_DepthRange.y);
}
