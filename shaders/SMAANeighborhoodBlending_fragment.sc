$input v_texcoord0, v_texcoord2

#include <bgfx_shader.sh>

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "SMAA.sh"

SAMPLER2D(s_SmaaColor, 0);
SAMPLER2D(s_SmaaBlend, 1);

void main()
{
#if BGFX_SHADER_LANGUAGE_GLSL
	gl_FragColor = SMAANeighborhoodBlendingPS(v_texcoord0, v_texcoord2, s_SmaaColor, s_SmaaBlend);
#else
	gl_FragColor = SMAANeighborhoodBlendingPS(v_texcoord0, v_texcoord2, s_SmaaColor.m_texture, s_SmaaBlend.m_texture);
#endif
}
