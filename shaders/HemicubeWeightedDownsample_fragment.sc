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
SAMPLER2D(u_HemicubeWeights, 1);

uniform vec4 u_HemicubeWeightsTextureSize; // only x and y used

vec4 weightedSample(ivec2 h_uv, ivec2 w_uv, ivec2 quadrant)
{
	vec4 sample = texelFetch(u_HemicubeAtlas, h_uv + quadrant, 0);
	vec2 weight = texelFetch(u_HemicubeWeights, w_uv + quadrant, 0).rg;
	//return vec4(sample.rgb * weight.r, sample.a * weight.g);
	return sample;
}

vec4 threeWeightedSamples(ivec2 h_uv, ivec2 w_uv, ivec2 offset)
{ 
	// horizontal triple sum
	vec4 sum = weightedSample(h_uv, w_uv, offset);
	offset.x += 2;
	sum += weightedSample(h_uv, w_uv, offset);
	offset.x += 2;
	sum += weightedSample(h_uv, w_uv, offset);
	return sum / 3.0;
}

void main()
{
	// this is a weighted sum downsampling pass (alpha component contains the weighted valid sample count)
	vec2 in_uv = (gl_FragCoord.xy - vec2_splat(0.5)) * vec2(6.0, 2.0) + vec2_splat(0.01);
	ivec2 h_uv = ivec2(in_uv);
	ivec2 w_uv = ivec2(mod(in_uv, u_HemicubeWeightsTextureSize.xy)); // there's no integer modulo :(
	vec4 lb = threeWeightedSamples(h_uv, w_uv, ivec2(0, 0));
	vec4 rb = threeWeightedSamples(h_uv, w_uv, ivec2(1, 0));
	vec4 lt = threeWeightedSamples(h_uv, w_uv, ivec2(0, 1));
	vec4 rt = threeWeightedSamples(h_uv, w_uv, ivec2(1, 1));
	gl_FragColor = (lb + rb + lt + rt) / 4.0;
}
