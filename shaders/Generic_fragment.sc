$input v_position, v_projPosition, v_texcoord0, v_texcoord1, v_normal, v_color0, v_viewDir

#include <bgfx_shader.sh>
#include "Common.sh"
#include "SharedDefines.sh"

SAMPLER2D(u_DiffuseMap, 0);
SAMPLER2D(u_LightMap, 1);
SAMPLER2D(u_NormalMap, 2);
SAMPLER2D(u_DeluxeMap, 3);
SAMPLER2D(u_SpecularMap, 4);

#if defined(USE_SOFT_SPRITE)
SAMPLER2D(u_DepthMap, 5);

uniform vec4 u_DepthRange;
uniform vec4 u_SoftSpriteDepth; // only x used
#endif

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

float Lambert(vec3 surfaceNormal, vec3 lightDir)
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

	if (int(u_LightType.x) == LIGHT_MAP)
	{
		gl_FragColor.rgb = diffuse.rgb * texture2D(u_LightMap, v_texcoord1).rgb * v_color0.rgb;
	}
	else if (int(u_LightType.x) == LIGHT_VECTOR)
	{
		vec3 lightColor = u_DirectedLight.xyz * v_color0.rgb;
		vec3 ambientColor = u_AmbientLight.xyz * v_color0.rgb;
		gl_FragColor.rgb = diffuse.rgb * (ambientColor + lightColor * Lambert(v_normal.xyz, u_LightDirection.xyz));
	}
	else // LIGHT_VERTEX or LIGHT_NONE
	{
		gl_FragColor.rgb = diffuse.rgb * v_color0.rgb;
	}

	if (int(u_LightType.x) != LIGHT_NONE)
	{
		for (int i = 0; i < int(u_NumDynamicLights.x); i++)
		{
			vec3 dir = u_DynamicLightPositions[i].xyz - v_position;
			vec3 color = u_DynamicLightColors[i].xyz;
			float intensity = u_DynamicLightColors[i].w;
			float attenuation = saturate(1.0 - length(dir) / intensity);
			gl_FragColor.rgb += diffuse.rgb * v_color0.rgb * (color * attenuation * Lambert(v_normal.xyz, normalize(dir)));
		}
	}

#if defined(USE_SOFT_SPRITE)
	// Normalized linear depths.
	float sceneDepth = texture2D(u_DepthMap, gl_FragCoord.xy * u_viewTexel.xy).r;

	// GL uses -1 to 1 NDC. D3D uses 0 to 1.
#if BGFX_SHADER_LANGUAGE_HLSL
	float fragmentDepth = ToLinearDepth(v_projPosition.z / v_projPosition.w, u_DepthRange.z, u_DepthRange.w);
#else
	float fragmentDepth = ToLinearDepth(v_projPosition.z / v_projPosition.w * 0.5 + 0.5, u_DepthRange.z, u_DepthRange.w);
#endif

	// Depth change in worldspace.
	float wsDelta = (sceneDepth - fragmentDepth) * (u_DepthRange.w - u_DepthRange.z);

	alpha *= saturate(wsDelta / u_SoftSpriteDepth.x);
#endif

	gl_FragColor.a = alpha;
}
