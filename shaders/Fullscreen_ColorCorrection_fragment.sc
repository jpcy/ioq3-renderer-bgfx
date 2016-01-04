$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_DiffuseMap, 0);

uniform vec4 u_BrightnessContrastGammaSaturation;
#define brightness u_BrightnessContrastGammaSaturation.x
#define contrast u_BrightnessContrastGammaSaturation.y
#define gamma u_BrightnessContrastGammaSaturation.z
#define saturation u_BrightnessContrastGammaSaturation.w

void main()
{
	vec3 color = (texture2D(u_DiffuseMap, v_texcoord0).rgb - 0.5) * contrast + 0.5 + brightness;
	vec3 intensity = vec3_splat(dot(color, vec3(0.2125, 0.7154, 0.0721)));
	color = mix(intensity, color, saturation);
	gl_FragColor = vec4(pow(color.r, gamma), pow(color.g, gamma), pow(color.b, gamma), 1.0);
}
