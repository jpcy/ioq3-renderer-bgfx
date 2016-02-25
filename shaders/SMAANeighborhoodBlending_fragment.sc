$input v_texcoord0, v_texcoord2

#include <bgfx_shader.sh>

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "SMAA.sh"

SAMPLER2D(u_SmaaColorSampler, 0);
SAMPLER2D(u_SmaaBlendSampler, 1);

void main()
{
#if BGFX_SHADER_LANGUAGE_HLSL
	gl_FragColor = SMAANeighborhoodBlendingPS(v_texcoord0, v_texcoord2, u_SmaaColorSampler.m_texture, u_SmaaBlendSampler.m_texture);
#else
	gl_FragColor = SMAANeighborhoodBlendingPS(v_texcoord0, v_texcoord2, u_SmaaColorSampler, u_SmaaBlendSampler);
#endif
}
