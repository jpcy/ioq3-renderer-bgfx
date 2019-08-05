uniform vec4 u_SmaaMetrics;

#define SMAA_PRESET_MEDIUM
#define SMAA_RT_METRICS u_SmaaMetrics

#if BGFX_SHADER_LANGUAGE_GLSL
#define SMAA_GLSL_3
#else
#define SMAA_HLSL_4
#endif

#include "SMAA.hlsl"
