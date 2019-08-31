$input v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4

#include <bgfx_shader.sh>

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "SMAA.sh"

SAMPLER2D(s_SmaaEdges, 0);
SAMPLER2D(s_SmaaArea, 1);
SAMPLER2D(s_SmaaSearch, 2);

void main()
{
	vec4 offset[3];
	offset[0] = v_texcoord2;
	offset[1] = v_texcoord3;
	offset[2] = v_texcoord4;
	vec4 subsampleIndices = vec4_splat(0.0);
#if BGFX_SHADER_LANGUAGE_GLSL
	gl_FragColor = SMAABlendingWeightCalculationPS(v_texcoord0, v_texcoord1, offset, s_SmaaEdges, s_SmaaArea, s_SmaaSearch, subsampleIndices);
#else
	gl_FragColor = SMAABlendingWeightCalculationPS(v_texcoord0, v_texcoord1, offset, s_SmaaEdges.m_texture, s_SmaaArea.m_texture, s_SmaaSearch.m_texture, subsampleIndices);
#endif
}
