$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_Texture, 0);

uniform vec4 u_Color;

void main()
{
	gl_FragColor = texture2D(s_Texture, v_texcoord0) * u_Color;
}
