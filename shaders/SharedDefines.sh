// Defines shared between C++ and shader code
#define AGEN_IDENTITY          0
#define AGEN_LIGHTING_SPECULAR 1
#define AGEN_PORTAL            2
#define AGEN_WATER             3

#define ATEST_GT_0   1
#define ATEST_LT_128 2
#define ATEST_GE_128 3

#define CGEN_IDENTITY         1
#define CGEN_EXACT_VERTEX     6
#define CGEN_VERTEX           7

#define DGEN_NONE        0
#define DGEN_BULGE       1
#define DGEN_MOVE        2
#define DGEN_WAVE        3

#define DGEN_WAVE_NONE             0
#define DGEN_WAVE_SIN              1
#define DGEN_WAVE_SQUARE           2
#define DGEN_WAVE_TRIANGLE         3
#define DGEN_WAVE_SAWTOOTH         4
#define DGEN_WAVE_INVERSE_SAWTOOTH 5

#define DLIGHT_CAPSULE 0
#define DLIGHT_POINT   1

#define LIGHT_NONE   0
#define LIGHT_MAP    1
#define LIGHT_VERTEX 3
#define LIGHT_VECTOR 4

#define GEN_ALPHA    0
#define GEN_COLOR    1
#define GEN_TEXCOORD 2

#define MAX_DEFORMS 3

#define RENDER_MODE_NONE     0
#define RENDER_MODE_LIT      1
#define RENDER_MODE_LIGHTMAP 2

#define RGBM_MAX_RANGE 8.0

#define TCGEN_NONE               0
#define TCGEN_ENVIRONMENT_MAPPED 1
#define TCGEN_FOG                2
#define TCGEN_FRAGMENT           3
#define TCGEN_LIGHTMAP           4
#define TCGEN_TEXTURE            5
#define TCGEN_VECTOR             6

#define TEXTURE_DEBUG_R    0
#define TEXTURE_DEBUG_G    1
#define TEXTURE_DEBUG_B    2
#define TEXTURE_DEBUG_RG   3
#define TEXTURE_DEBUG_LINEAR_DEPTH 4

#define TU_DIFFUSE               0
#define TU_DIFFUSE2              1
#define TU_LIGHT                 2
#define TU_DEPTH                 3
#define TU_DYNAMIC_LIGHT_CELLS   4
#define TU_DYNAMIC_LIGHT_INDICES 5
#define TU_DYNAMIC_LIGHTS        6
#define TU_SHADOWMAP             7
#define TU_NOISE                 8

#define USE_HALF_LAMBERT