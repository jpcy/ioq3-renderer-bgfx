$input v_position, v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_PortalClip;
uniform vec4 u_PortalPlane;
#define v_scale v_texcoord0.x
uniform vec4 u_Color;

void main()
{
	if (u_PortalClip.x == 1.0)
	{
		float dist = dot(v_position, u_PortalPlane.xyz) - u_PortalPlane.w;

		if (dist < 0.0)
			discard;
	}

	gl_FragColor = vec4(u_Color.rgb, sqrt(clamp(v_scale, 0.0, 1.0)));
}
