$input a_position, a_normal, a_texcoord0
$output v_position, v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"
#include "Gen_Deform.sh"

#define v_scale v_texcoord0.x
uniform vec4 u_Color;
uniform vec4 u_FogDistance;
uniform vec4 u_FogDepth;
uniform vec4 u_FogEyeT; // only x used
uniform vec4 u_DepthRangeEnabled; // only x used
uniform vec4 u_DepthRange;
uniform vec4 u_Time; // only x used

void main()
{
	v_position = mul(u_model[0], vec4(a_position, 1.0)).xyz;

	if (int(u_NumDeforms.x) > 0)
	{
		CalculateDeform(v_position, a_normal, a_texcoord0.xy, u_Time.x);
	}

	vec4 projPosition = mul(u_viewProj, vec4(v_position, 1.0));
	if (int(u_DepthRangeEnabled.x) != 0)
		projPosition = ApplyDepthRange(projPosition, u_DepthRange.x, u_DepthRange.y);
	gl_Position = projPosition;
	v_scale = CalcFog(a_position, u_FogDepth, u_FogDistance, u_FogEyeT.x) * u_Color.a * u_Color.a; // NOTE: fog wants modelspace position. Should really deform it too, but the difference isn't enough to matter.
}
