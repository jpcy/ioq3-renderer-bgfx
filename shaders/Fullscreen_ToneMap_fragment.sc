$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_DiffuseMap, 0);

uniform vec4 u_BrightnessContrastGammaSaturation;
#define brightness u_BrightnessContrastGammaSaturation.x
#define contrast u_BrightnessContrastGammaSaturation.y
#define gamma u_BrightnessContrastGammaSaturation.z
#define saturation u_BrightnessContrastGammaSaturation.w

vec3 Reinhard(vec3 x)
{
	return x / (x + 1.0);
}

vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

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
	vec3 color = texture2D(u_DiffuseMap, v_texcoord0).rgb;

	// tone map
	color = saturate(color);

	// contrast and brightness
	color = (color - 0.5) * contrast + 0.5 + brightness;

	// saturation
	vec3 intensity = vec3_splat(dot(color, vec3(0.2125, 0.7154, 0.0721)));
	color = mix(intensity, color, saturation);

	// gamma
	color = vec3(pow(color.r, gamma), pow(color.g, gamma), pow(color.b, gamma));

	gl_FragColor = vec4(color, 1.0);
}
