$input v_texcoord0

#include <bgfx_shader.sh>
#include "Common.sh"

SAMPLER2D(u_TextureSampler, 0);
SAMPLER2D(u_AdaptedLuminanceSampler, 1);

uniform vec4 u_BrightnessContrastGammaSaturation;
#define brightness u_BrightnessContrastGammaSaturation.x
#define contrast u_BrightnessContrastGammaSaturation.y
#define gamma u_BrightnessContrastGammaSaturation.z
#define saturation u_BrightnessContrastGammaSaturation.w

uniform vec4 u_HdrExposure; // only x used

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

void main()
{
	vec3 color = texture2D(u_TextureSampler, v_texcoord0).rgb;

	// tone map
	color = ACESFilm(color * u_HdrExposure.x);

	// contrast and brightness
	color = (color - 0.5) * contrast + 0.5 + brightness;

	// saturation
	vec3 intensity = vec3_splat(dot(color, vec3(0.2125, 0.7154, 0.0721)));
	color = mix(intensity, color, saturation);

	// gamma
	//color = pow(color, vec3_splat(1.0 / gamma));

	gl_FragColor = vec4(color, 1.0);
}
