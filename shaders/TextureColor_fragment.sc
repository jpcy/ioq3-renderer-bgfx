$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_TextureSampler, 0);

uniform vec4 u_Color;

void main()
{
	gl_FragColor = texture2D(u_TextureSampler, v_texcoord0) * u_Color;
}
