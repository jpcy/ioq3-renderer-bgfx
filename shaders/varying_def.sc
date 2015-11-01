vec2 v_texcoord0       : TEXCOORD0 = vec2(0.0, 0.0);
vec2 v_texcoord1       : TEXCOORD1 = vec2(0.0, 0.0);
vec3 v_viewDir         : TEXCOORD2 = vec3(0.0, 0.0, 0.0);
vec3 v_position        : TEXCOORD3 = vec3(0.0, 0.0, 0.0);
vec4 v_normal          : NORMAL    = vec4(0.0, 0.0, 1.0, 0.0);
vec4 v_tangent         : TANGENT   = vec4(1.0, 0.0, 0.0, 0.0);
vec4 v_bitangent       : BINORMAL  = vec4(0.0, 1.0, 0.0, 0.0);
vec4 v_color0          : COLOR0    = vec4(1.0, 1.0, 1.0, 1.0);

vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec3 a_tangent   : TANGENT;
vec2 a_texcoord0 : TEXCOORD0;
vec2 a_texcoord1 : TEXCOORD1;
vec3 a_texcoord2 : TEXCOORD2;
vec4 a_color0    : COLOR0;
