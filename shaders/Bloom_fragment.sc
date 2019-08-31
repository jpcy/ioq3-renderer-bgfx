$input v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"

SAMPLER2D(s_Texture, 0);
SAMPLER2D(s_Bloom, 1);

uniform vec4 u_Bloom_Write_Scale;
#define u_BloomScale u_Bloom_Write_Scale.y

void main()
{
	vec3 color = texture2D(s_Texture, v_texcoord0).rgb + texture2D(s_Bloom, v_texcoord0).rgb * u_BloomScale;
	gl_FragColor = vec4(color, 1.0);
}
