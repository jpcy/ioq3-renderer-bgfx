$input v_position, v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(u_DiffuseMap, 0);

uniform vec4 u_PortalClip;
uniform vec4 u_PortalPlane;

#if defined(USE_ALPHA_TEST)
uniform vec4 u_AlphaTest; // only x used
#endif

void main()
{
	if (u_PortalClip.x == 1.0)
	{
		float dist = dot(v_position, u_PortalPlane.xyz) - u_PortalPlane.w;

		if (dist < 0.0)
			discard;
	}

	vec4 diffuse = texture2D(u_DiffuseMap, v_texcoord0) * v_color0;

#if defined(USE_ALPHA_TEST)
	if (u_AlphaTest.x == ATEST_GT_0)
	{
		if (diffuse.a <= 0.0)
			discard;
	}
	else if (u_AlphaTest.x == ATEST_LT_128)
	{
		if (diffuse.a >= 0.5)
			discard;
	}
	else if (u_AlphaTest.x == ATEST_GE_128)
	{
		if (diffuse.a < 0.5)
			discard;
	}
#endif

	gl_FragColor = diffuse;
}
