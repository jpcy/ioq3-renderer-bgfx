$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_DiffuseMap, 0);

void main()
{
	float color = texture2D(u_DiffuseMap, v_texcoord0).r;
	gl_FragColor = vec4(color, color, color, 1.0);
}
