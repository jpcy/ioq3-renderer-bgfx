$input a_position, a_normal, a_texcoord0
$output v_position, v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"
#include "Gen_Deform.sh"
#include "Gen_Tex.sh"

#if defined(USE_ALPHA_TEST)
uniform vec4 u_Generators;
#define u_TCGen0 int(u_Generators[GEN_TEXCOORD])
uniform vec4 u_DiffuseTexMatrix;
uniform vec4 u_DiffuseTexOffTurb;
#endif

uniform vec4 u_DepthRange; // x is offset, y is scale
uniform vec4 u_Time; // only x used

void main()
{
	vec3 position = a_position;

	if (int(u_NumDeforms_AutoSprite.x) > 0)
	{
		CalculateDeform(position, a_normal, a_texcoord0, u_Time.x);
	}

#if defined(USE_ALPHA_TEST)
	if (u_TCGen0 != TCGEN_NONE)
	{
		v_texcoord0 = ModTexCoords(a_texcoord0, position, u_DiffuseTexMatrix, u_DiffuseTexOffTurb);
	}
	else
	{
		v_texcoord0 = a_texcoord0;
	}
#else
	v_texcoord0 = a_texcoord0;
#endif

	v_position = mul(u_model[0], vec4(position, 1.0)).xyz;
	vec4 projPosition = mul(u_viewProj, vec4(v_position, 1.0));

#if defined(USE_DEPTH_RANGE)
	projPosition = ApplyDepthRange(projPosition, u_DepthRange.x, u_DepthRange.y);
#endif

	gl_Position = projPosition;
}
