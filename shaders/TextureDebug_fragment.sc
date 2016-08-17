$input v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"

SAMPLER2D(u_TextureSampler, 0);

uniform vec4 u_TextureDebug; // only x used.

void main()
{
	vec4 tex = texture2D(u_TextureSampler, v_texcoord0);

	if (int(u_TextureDebug.x) == TEXTURE_DEBUG_SINGLE_CHANNEL)
	{
		gl_FragColor = vec4(tex.r, tex.r, tex.r, 1.0);
	}
	else if (int(u_TextureDebug.x) == TEXTURE_DEBUG_RGBM)
	{
		gl_FragColor = vec4(DecodeRGBM(tex), 1.0);
	}
	else
	{
		gl_FragColor = tex;
	}
}
