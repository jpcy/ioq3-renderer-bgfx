$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_LuminanceSampler, 9);
SAMPLER2D(u_AdaptedLuminanceSampler, 10);

uniform vec4 u_Time; // only x used

void main()
{
	float current = texture2D(u_LuminanceSampler, v_texcoord0).r;
	float oldAdapted = texture2D(u_AdaptedLuminanceSampler, v_texcoord0).r;

	// DX SDK HDRLighting sample:
	// The user's adapted luminance level is simulated by closing the gap between
    // adapted luminance and current luminance by 2% every frame, based on a
    // 30 fps rate. This is not an accurate model of human adaptation, which can
    // take longer than half an hour.
	float newAdapted = oldAdapted + (current - oldAdapted) * (1.0 - pow(0.98f, 30 * u_Time.x));

	gl_FragColor = vec4(newAdapted, 0.0, 0.0, 0.0);
}
