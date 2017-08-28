$input v_position, v_texcoord0

#include <bgfx_shader.sh>
#include "PortalClip.sh"

uniform vec4 u_Bloom_Enabled_Write_Scale;
#define u_BloomEnabled int(u_Bloom_Enabled_Write_Scale.x)
#define v_scale v_texcoord0.x
uniform vec4 u_Color;

void main()
{
	if (PortalClipped(v_position))
		discard;
	vec4 fragColor = vec4(u_Color.rgb, sqrt(saturate(v_scale)));
	gl_FragData[0] = fragColor;
	if (u_BloomEnabled != 0)
		gl_FragData[1] = vec4(0.0, 0.0, 0.0, fragColor.a);
}
