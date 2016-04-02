$input v_texcoord0

/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */
 
#include <bgfx_shader.sh>

SAMPLER2D(u_TextureSampler, 0);

uniform vec4 u_TexelOffsets[16];

void main()
{
	float sum;
	sum  = texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 0].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 1].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 2].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 3].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 4].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 5].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 6].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 7].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 8].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[ 9].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[10].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[11].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[12].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[13].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[14].xy).r;
	sum += texture2D(u_TextureSampler, v_texcoord0 + u_TexelOffsets[15].xy).r;
	gl_FragColor = vec4(sum / 16.0, 0.0, 0.0, 0.0);
}
