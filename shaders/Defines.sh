// Defines shared between C++ and shader code
#define AGEN_IDENTITY          0
#define AGEN_LIGHTING_SPECULAR 1
#define AGEN_PORTAL            2

#define ATEST_GT_0   1
#define ATEST_LT_128 2
#define ATEST_GE_128 3

#define CGEN_IDENTITY         1
#define CGEN_LIGHTING_DIFFUSE 2

#define DGEN_NONE  0
#define DGEN_WAVE  1
#define DGEN_BULGE 2
#define DGEN_MOVE  3

#define DGEN_WAVE_NONE             0
#define DGEN_WAVE_SIN              1
#define DGEN_WAVE_SQUARE           2
#define DGEN_WAVE_TRIANGLE         3
#define DGEN_WAVE_SAWTOOTH         4
#define DGEN_WAVE_INVERSE_SAWTOOTH 5

#define LIGHT_NONE   0
#define LIGHT_MAP    1
#define LIGHT_VERTEX 3
#define LIGHT_VECTOR 4

#define GEN_ALPHA    0
#define GEN_COLOR    1
#define GEN_TEXCOORD 2

#define MAX_DEFORMS 3
#define MAX_DLIGHTS 32

#define TCGEN_NONE               0
#define TCGEN_ENVIRONMENT_MAPPED 1
#define TCGEN_FOG                2
#define TCGEN_LIGHTMAP           3
#define TCGEN_TEXTURE            4
#define TCGEN_VECTOR             5