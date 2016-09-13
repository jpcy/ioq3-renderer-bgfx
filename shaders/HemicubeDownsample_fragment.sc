$input v_texcoord0

/***********************************************************
* A single header file OpenGL lightmapping library         *
* https://github.com/ands/lightmapper                      *
* no warranty implied | use at your own risk               *
* author: Andreas Mantler (ands) | last change: 23.07.2016 *
*                                                          *
* License:                                                 *
* This software is in the public domain.                   *
* Where that dedication is not recognized,                 *
* you are granted a perpetual, irrevocable license to copy *
* and modify this file however you want.                   *
***********************************************************/

#include <bgfx_shader.sh>

SAMPLER2D(u_HemicubeAtlas, 0);

void main()
{
	// this is a sum downsampling pass (alpha component contains the weighted valid sample count)
	ivec2 h_uv = ivec2((gl_FragCoord.xy - vec2_splat(0.5)) * 2.0 + vec2_splat(0.01));
	vec4 lb = texelFetch(u_HemicubeAtlas, h_uv + ivec2(0, 0), 0);
	vec4 rb = texelFetch(u_HemicubeAtlas, h_uv + ivec2(1, 0), 0);
	vec4 lt = texelFetch(u_HemicubeAtlas, h_uv + ivec2(0, 1), 0);
	vec4 rt = texelFetch(u_HemicubeAtlas, h_uv + ivec2(1, 1), 0);
	gl_FragColor = (lb + rb + lt + rt) / 4.0;
}
