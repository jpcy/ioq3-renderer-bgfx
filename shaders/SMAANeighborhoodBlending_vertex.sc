$input a_position, a_texcoord0
$output v_texcoord0, v_texcoord2

#include <bgfx_shader.sh>

#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
#include "SMAA.sh"

void main()
{
	vec4 offset;
	SMAANeighborhoodBlendingVS(a_texcoord0.xy, offset);
	v_texcoord0 = a_texcoord0.xy;
	v_texcoord2 = offset;
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
