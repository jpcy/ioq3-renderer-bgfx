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

SAMPLER2D(u_DynamicLightsSampler, 6);

uniform vec4 u_PortalClip;
uniform vec4 u_PortalPlane;

#if defined(USE_ALPHA_TEST)
uniform vec4 u_AlphaTest; // only x used
#endif

uniform vec4 u_Generators;
#define u_ColorGen int(u_Generators[GEN_COLOR])

// light vector
uniform vec4 u_LightDirection;
uniform vec4 u_DirectedLight;
uniform vec4 u_AmbientLight;
uniform vec4 u_NormalScale;
uniform vec4 u_SpecularScale;

uniform vec4 u_LightType; // only x used

uniform vec4 u_DynamicLights_Num_TextureWidth; // x is the number of dynamic lights, y is the texture width

uniform vec4 u_OverbrightFactor; // only x used

float Lambert(vec3 surfaceNormal, vec3 lightDir)
{
#if defined(USE_HALF_LAMBERT)
	return pow(dot(surfaceNormal, lightDir) * 0.5 + 0.5, 2);
#else
	return saturate(dot(surfaceNormal, lightDir));
#endif
}

vec4 FetchDynamicLightData(int offset)
{
	int u = offset % int(u_DynamicLights_Num_TextureWidth.y);
	int v = offset / int(u_DynamicLights_Num_TextureWidth.y);

#if BGFX_SHADER_LANGUAGE_HLSL
	return u_DynamicLightsSampler.m_texture.Load(ivec3(u, v, 0));
#else
	return texelFetch(u_DynamicLightsSampler, ivec2(u, v), 0);
#endif
}

// From http://stackoverflow.com/a/9557244
vec3 ClosestPointOnLineSegment(vec3 A, vec3 B, vec3 P)
{
	vec3 AP = P - A;       //Vector from A to P   
    vec3 AB = B - A;       //Vector from A to B  

    float magnitudeAB = length(AB) * length(AB);     //Magnitude of AB vector (it's length squared)     
    float ABAPproduct = dot(AP, AB);    //The DOT product of a_to_p and a_to_b     
    float distance = ABAPproduct / magnitudeAB; //The normalized "distance" from a to your closest point  
	return A + AB * saturate(distance);
}

void main()
{
	if (u_PortalClip.x == 1.0)
	{
		float dist = dot(v_position, u_PortalPlane.xyz) - u_PortalPlane.w;

		if (dist < 0.0)
			discard;
	}

	vec4 diffuse = ToLinear(texture2D(u_DiffuseMap, v_texcoord0));
	float alpha = diffuse.a * v_color0.a;

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

	gl_FragColor.a = alpha;
	vec3 diffuseLight = vec3_splat(0.0);

	if (int(u_LightType.x) == LIGHT_MAP)
	{
		diffuseLight = ToLinear(texture2D(u_LightMap, v_texcoord1).rgb * u_OverbrightFactor.x);
	}
	else if (int(u_LightType.x) == LIGHT_VECTOR)
	{
		diffuseLight = u_AmbientLight.xyz + u_DirectedLight.xyz * Lambert(v_normal.xyz, u_LightDirection.xyz);
	}
	else // LIGHT_VERTEX or LIGHT_NONE
	{
		diffuseLight = vec3_splat(1.0);
	}

	if (int(u_LightType.x) != LIGHT_NONE || u_ColorGen == CGEN_EXACT_VERTEX || u_ColorGen == CGEN_VERTEX)
	{
		for (int i = 0; i < int(u_DynamicLights_Num_TextureWidth.x); i++)
		{
			vec4 light_color_radius = FetchDynamicLightData(i * 3 + 1);
			vec4 light_position_type = FetchDynamicLightData(i * 3 + 2);
			vec3 dir;

			if (int(light_position_type.w) == DLIGHT_POINT)
			{
				dir = light_position_type.xyz - v_position;
			}
			else // DLIGHT_CAPSULE
			{
				vec3 capsuleEnd = FetchDynamicLightData(i * 3 + 0).xyz;
				vec3 pointOnLine = ClosestPointOnLineSegment(light_position_type.xyz, capsuleEnd, v_position);
				dir = pointOnLine - v_position;
			}

			float attenuation = saturate(1.0 - length(dir) / light_color_radius.w);
			diffuseLight += light_color_radius.rgb * attenuation * Lambert(v_normal.xyz, normalize(dir));
		}
	}

	gl_FragColor.rgb = ToGamma(diffuse.rgb * v_color0.rgb * diffuseLight);
}
