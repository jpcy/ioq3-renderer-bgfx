$input a_position, a_normal, a_tangent, a_texcoord0, a_texcoord1, a_texcoord2, a_color0
$output v_position, v_texcoord0, v_texcoord1, v_normal, v_color0, v_viewDir

#include <bgfx_shader.sh>
#include "Common.sh"
#include "Generators.sh"

uniform vec4 u_DepthRange;
uniform vec4 u_LightType; // only x used
uniform vec4 u_BaseColor;
uniform vec4 u_VertColor;
uniform vec4 u_ViewOrigin;

void main()
{
	v_position = a_position;
	vec3 normal = a_normal;

	if (u_TCGen0 != TCGEN_NONE)
	{
		vec2 tex = GenTexCoords(v_position, normal, a_texcoord0, a_texcoord1);
		v_texcoord0 = ModTexCoords(tex, v_position, u_DiffuseTexMatrix, u_DiffuseTexOffTurb);
	}
	else
	{
		v_texcoord0 = a_texcoord0;
	}

	v_texcoord1 = a_texcoord1;
	v_color0 = u_VertColor * a_color0 + u_BaseColor;
	v_position  = mul(u_model[0], vec4(v_position, 1.0)).xyz;
	v_normal = mul(u_model[0], vec4(normal, 0.0));
	v_viewDir = u_ViewOrigin.xyz - v_position;
	gl_Position = ApplyDepthRange(mul(u_viewProj, vec4(v_position, 1.0)), u_DepthRange.x, u_DepthRange.y);
}
