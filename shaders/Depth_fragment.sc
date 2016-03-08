$input v_position, v_texcoord0, v_color0

#include <bgfx_shader.sh>
#include "SharedDefines.sh"

#if defined(USE_ALPHA_TEST)
SAMPLER2D(u_TextureSampler, 0);

uniform vec4 u_AlphaTest; // only x used
#endif

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

#if defined(USE_ALPHA_TEST)
	float alpha = texture2D(u_TextureSampler, v_texcoord0).a * v_color0.a;

	if (int(u_AlphaTest.x) == ATEST_GT_0)
	{
		if (alpha <= 0.0)
			discard;
	}
	else if (int(u_AlphaTest.x) == ATEST_LT_128)
	{
		if (alpha >= 0.5)
			discard;
	}
	else if (int(u_AlphaTest.x) == ATEST_GE_128)
	{
		if (alpha < 0.5)
			discard;
	}
#endif

	gl_FragColor = vec4_splat(0.0);
}
