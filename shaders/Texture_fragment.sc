$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_DiffuseMap, 0);

void main()
{
	gl_FragColor = texture2D(u_DiffuseMap, v_texcoord0);
}
