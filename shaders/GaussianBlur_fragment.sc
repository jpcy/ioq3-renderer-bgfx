$input v_texcoord0

/*
https://github.com/Jam3/glsl-fast-gaussian-blur
The MIT License (MIT)
Copyright (c) 2015 Jam3

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <bgfx_shader.sh>

SAMPLER2D(u_TextureSampler, 0);

uniform vec4 u_GuassianBlurDirection; // only xy used

void main()
{
	vec4 color = vec4_splat(0.0);

#if 0
	vec2 off1 = vec2_splat(1.3333333333333333) * u_GuassianBlurDirection.xy;
	color += texture2D(u_TextureSampler, v_texcoord0) * 0.29411764705882354;
	color += texture2D(u_TextureSampler, v_texcoord0 + (off1 / u_viewRect.zw)) * 0.35294117647058826;
	color += texture2D(u_TextureSampler, v_texcoord0 - (off1 / u_viewRect.zw)) * 0.35294117647058826;
#else
	vec2 off1 = vec2_splat(1.3846153846) * u_GuassianBlurDirection.xy;
	vec2 off2 = vec2_splat(3.2307692308) * u_GuassianBlurDirection.xy;
	color += texture2D(u_TextureSampler, v_texcoord0) * 0.2270270270;
	color += texture2D(u_TextureSampler, v_texcoord0 + (off1 / u_viewRect.zw)) * 0.3162162162;
	color += texture2D(u_TextureSampler, v_texcoord0 - (off1 / u_viewRect.zw)) * 0.3162162162;
	color += texture2D(u_TextureSampler, v_texcoord0 + (off2 / u_viewRect.zw)) * 0.0702702703;
	color += texture2D(u_TextureSampler, v_texcoord0 - (off2 / u_viewRect.zw)) * 0.0702702703;
#endif

	gl_FragColor = color;
}
