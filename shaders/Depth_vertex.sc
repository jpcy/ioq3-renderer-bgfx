$input a_position, a_normal, a_texcoord0
$output v_position, v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"
#include "Deform.sh"

uniform vec4 u_DepthRange; // x is offset, y is scale
uniform vec4 u_Time; // only x used

void main()
{
	vec3 position = a_position;

	if (int(u_NumDeforms.x) > 0)
	{
		CalculateDeform(position, a_normal, a_texcoord0, u_Time.x);
	}

	v_texcoord0 = a_texcoord0;
	v_position = mul(u_model[0], vec4(position, 1.0)).xyz;
	gl_Position = ApplyDepthRange(mul(u_viewProj, vec4(v_position, 1.0)), u_DepthRange.x, u_DepthRange.y);
}
