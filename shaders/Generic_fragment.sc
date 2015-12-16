$input v_position, v_texcoord0, v_texcoord1, v_normal, v_color0, v_viewDir

#include <bgfx_shader.sh>
#include "Common.sh"
#include "Defines.sh"

SAMPLER2D(u_DiffuseMap, 0);
SAMPLER2D(u_LightMap, 1);
SAMPLER2D(u_NormalMap, 2);
SAMPLER2D(u_DeluxeMap, 3);
SAMPLER2D(u_SpecularMap, 4);
SAMPLER2D(u_ShadowMap, 5);
SAMPLERCUBE(u_CubeMap, 6);

uniform vec4 u_PortalClip;
uniform vec4 u_PortalPlane;

#if defined(USE_ALPHA_TEST)
uniform vec4 u_AlphaTest; // only x used
#endif

// light vector
uniform vec4 u_LightDirection;
uniform vec4 u_DirectedLight;
uniform vec4 u_AmbientLight;
uniform vec4 u_NormalScale;
uniform vec4 u_SpecularScale;

uniform vec4 u_LightType; // only x used

uniform vec4 u_NumDynamicLights; // only x used
uniform vec4 u_DynamicLightColors[MAX_DLIGHTS];
uniform vec4 u_DynamicLightPositions[MAX_DLIGHTS];

float lambert(vec3 surfaceNormal, vec3 lightDir)
{
	return saturate(dot(surfaceNormal, lightDir));
}

void main()
{
	if (u_PortalClip.x == 1.0)
	{
		float dist = dot(v_position, u_PortalPlane.xyz) - u_PortalPlane.w;

		if (dist < 0.0)
			discard;
	}

	vec4 diffuse = texture2D(u_DiffuseMap, v_texcoord0);
	float alpha = diffuse.a * v_color0.a;

#if defined(USE_ALPHA_TEST)
	if (u_AlphaTest.x == ATEST_GT_0)
	{
		if (alpha <= 0.0)
			discard;
	}
	else if (u_AlphaTest.x == ATEST_LT_128)
	{
		if (alpha >= 0.5)
			discard;
	}
	else if (u_AlphaTest.x == ATEST_GE_128)
	{
		if (alpha < 0.5)
			discard;
	}
#endif

	if (u_LightType.x == LIGHT_MAP)
	{
		gl_FragColor.rgb = diffuse.rgb * texture2D(u_LightMap, v_texcoord1).rgb * v_color0.rgb;
	}
	else if (u_LightType.x == LIGHT_VECTOR)
	{
		vec3 lightColor = u_DirectedLight.xyz * v_color0.rgb;
		vec3 ambientColor = u_AmbientLight.xyz * v_color0.rgb;
		gl_FragColor.rgb = diffuse.rgb * (ambientColor + lightColor * lambert(v_normal.xyz, u_LightDirection.xyz));
	}
	else // LIGHT_VERTEX or LIGHT_NONE
	{
		gl_FragColor.rgb = diffuse.rgb * v_color0.rgb;
	}

	if (u_LightType.x != LIGHT_NONE)
	{
		for (int i = 0; i < u_NumDynamicLights.x; i++)
		{
			vec3 dir = u_DynamicLightPositions[i].xyz - v_position;
			vec3 color = u_DynamicLightColors[i].xyz;
			float intensity = u_DynamicLightColors[i].w;
			float attenuation = saturate(1.0 - length(dir) / intensity);
			gl_FragColor.rgb += diffuse.rgb * v_color0.rgb * (color * attenuation * lambert(v_normal.xyz, normalize(dir)));
		}
	}

	gl_FragColor.a = alpha;
}
