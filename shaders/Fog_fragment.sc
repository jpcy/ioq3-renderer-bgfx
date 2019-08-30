$input v_position, v_texcoord0

#include <bgfx_shader.sh>
#include "PortalClip.sh"

#define v_scale v_texcoord0.x
uniform vec4 u_Color;

void main()
{
	if (PortalClipped(v_position))
		discard;
	vec4 fragColor = vec4(u_Color.rgb, sqrt(saturate(v_scale)));
#if defined(USE_BLOOM)
	gl_FragData[0] = fragColor;
	gl_FragData[1] = vec4(0.0, 0.0, 0.0, fragColor.a);
#else
	gl_FragColor = fragColor;
#endif
}
