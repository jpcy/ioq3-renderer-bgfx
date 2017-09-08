$input v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"

SAMPLER2D(u_TextureSampler, 0);

uniform vec4 u_DepthRange;
uniform vec4 u_TextureDebug; // only x used.

void main()
{
	vec4 tex = texture2D(u_TextureSampler, v_texcoord0);
	int debug = int(u_TextureDebug.x);

	if (debug == TEXTURE_DEBUG_R)
	{
		gl_FragColor = vec4(tex.r, tex.r, tex.r, 1.0);
	}
	else if (debug == TEXTURE_DEBUG_G)
	{
		gl_FragColor = vec4(tex.g, tex.g, tex.g, 1.0);
	}
	else if (debug == TEXTURE_DEBUG_B)
	{
		gl_FragColor = vec4(tex.b, tex.b, tex.b, 1.0);
	}
	else if (debug == TEXTURE_DEBUG_RG)
	{
		gl_FragColor = vec4(tex.r, tex.g, 0.0, 1.0);
	}
	else if (debug == TEXTURE_DEBUG_LINEAR_DEPTH)
	{
		gl_FragColor = vec4(vec3_splat(ToLinearDepth(tex.r, u_DepthRange.z, u_DepthRange.w)), 1.0);
	}
	else
	{
		gl_FragColor = tex;
	}
}
