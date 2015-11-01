$input v_position, v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(u_DiffuseMap, 0);

uniform vec4 u_PortalClip;
uniform vec4 u_PortalPlane;

void main()
{
	if (u_PortalClip.x == 1.0)
	{
		float dist = dot(v_position, u_PortalPlane.xyz) - u_PortalPlane.w;

		if (dist < 0.0)
			discard;
	}

	gl_FragColor = texture2D(u_DiffuseMap, v_texcoord0) * v_color0;
}
