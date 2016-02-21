$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_TextureSampler, 0);

void main()
{
	gl_FragColor = texture2D(u_TextureSampler, v_texcoord0);
}
