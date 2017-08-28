$input v_position, v_texcoord0, v_color0

#include <bgfx_shader.sh>
#include "SharedDefines.sh"
#include "AlphaTest.sh"
#include "PortalClip.sh"

#if defined(USE_ALPHA_TEST)
SAMPLER2D(u_TextureSampler, 0);
#endif

void main()
{
	if (PortalClipped(v_position))
		discard;

#if defined(USE_ALPHA_TEST)
	float alpha = texture2D(u_TextureSampler, v_texcoord0).a * v_color0.a;
	if (!AlphaTestPassed(alpha))
		discard;
#endif

	gl_FragColor = vec4_splat(0.0);
}
