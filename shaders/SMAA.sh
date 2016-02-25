uniform vec4 u_SmaaMetrics;

#define SMAA_PRESET_MEDIUM
#define SMAA_RT_METRICS u_SmaaMetrics

#if BGFX_SHADER_LANGUAGE_HLSL
#define SMAA_HLSL_4
#else
#define SMAA_GLSL_3
#endif

#include "SMAA.hlsl"
