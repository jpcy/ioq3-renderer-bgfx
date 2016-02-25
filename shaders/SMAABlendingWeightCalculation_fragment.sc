$input v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4

#include <bgfx_shader.sh>

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "SMAA.sh"

SAMPLER2D(u_SmaaEdgesSampler, 0);
SAMPLER2D(u_SmaaAreaSampler, 1);
SAMPLER2D(u_SmaaSearchSampler, 2);

void main()
{
	vec4 offset[3];
	offset[0] = v_texcoord2;
	offset[1] = v_texcoord3;
	offset[2] = v_texcoord4;
	vec4 subsampleIndices = vec4_splat(0.0);
#if BGFX_SHADER_LANGUAGE_HLSL
	gl_FragColor = SMAABlendingWeightCalculationPS(v_texcoord0, v_texcoord1, offset, u_SmaaEdgesSampler.m_texture, u_SmaaAreaSampler.m_texture, u_SmaaSearchSampler.m_texture, subsampleIndices);
#else
	gl_FragColor = SMAABlendingWeightCalculationPS(v_texcoord0, v_texcoord1, offset, u_SmaaEdgesSampler, u_SmaaAreaSampler, u_SmaaSearchSampler, subsampleIndices);
#endif
}
