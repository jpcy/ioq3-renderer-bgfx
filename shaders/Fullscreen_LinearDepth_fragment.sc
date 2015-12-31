$input v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"

SAMPLER2D(u_DepthMap, 5);

uniform vec4 u_DepthRange;

void main()
{
	float depth = ToLinearDepth(texture2D(u_DepthMap, v_texcoord0).r, u_DepthRange.z, u_DepthRange.w);
	gl_FragColor = vec4(depth, 0.0, 0.0, 1.0);
}
