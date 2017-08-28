$input v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"

SAMPLER2D(u_TextureSampler, 0);
SAMPLER2D(u_BloomSampler, 1);

uniform vec4 u_Bloom_Enabled_Write_Scale;
#define u_BloomScale u_Bloom_Enabled_Write_Scale.z

void main()
{
	vec3 color = texture2D(u_TextureSampler, v_texcoord0).rgb + texture2D(u_BloomSampler, v_texcoord0).rgb * u_BloomScale;
	gl_FragColor = vec4(color, 1.0);
}
