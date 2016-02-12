$input v_texcoord0

/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */
 
#include <bgfx_shader.sh>

SAMPLER2D(u_DiffuseMap, 0);

uniform vec4 u_TexelOffsets[16];

float Luma(vec3 rgb)
{
	return dot(vec3(0.2126729, 0.7151522, 0.0721750), rgb);
}

void main()
{
	vec3 rgb0 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[0].xy).rgb;
	vec3 rgb1 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[1].xy).rgb;
	vec3 rgb2 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[2].xy).rgb;
	vec3 rgb3 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[3].xy).rgb;
	vec3 rgb4 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[4].xy).rgb;
	vec3 rgb5 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[5].xy).rgb;
	vec3 rgb6 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[6].xy).rgb;
	vec3 rgb7 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[7].xy).rgb;
	vec3 rgb8 = texture2D(u_DiffuseMap, v_texcoord0 + u_TexelOffsets[8].xy).rgb;
	float sum = Luma(rgb0)
			  + Luma(rgb1)
			  + Luma(rgb2)
			  + Luma(rgb3)
			  + Luma(rgb4)
			  + Luma(rgb5)
			  + Luma(rgb6)
			  + Luma(rgb7)
			  + Luma(rgb8);
	gl_FragColor = vec4(sum / 9.0, 0.0, 0.0, 0.0);
}
