$input v_position, v_projPosition, v_texcoord0, v_texcoord1, v_normal, v_color0, v_viewDir

#include <bgfx_shader.sh>
#include "Common.sh"
#include "SharedDefines.sh"

SAMPLER2D(u_DiffuseMap, 0);
SAMPLER2D(u_LightMap, 1);

#if defined(USE_ALPHA_TEST)
uniform vec4 u_AlphaTest; // only x used
#endif

#if defined(USE_SOFT_SPRITE)
SAMPLER2D(u_DepthMap, 5);

uniform vec4 u_DepthRange;
uniform vec4 u_SoftSprite_Depth_UseAlpha; // only x and y used
#endif

#if defined(USE_DYNAMIC_LIGHTS)
USAMPLER2D(u_DynamicLightCellsSampler, 6);
USAMPLER2D(u_DynamicLightIndicesSampler, 7);
SAMPLER2D(u_DynamicLightsSampler, 8);

uniform vec4 u_DynamicLightCellSize; // xyz is size
uniform vec4 u_DynamicLightGridOffset; // w not used
uniform vec4 u_DynamicLightGridSize; // w not used
uniform vec4 u_DynamicLightNum; // x is the number of dynamic lights
uniform vec4 u_DynamicLightTextureSizes_Cells_Indices_Lights; // w not used
#endif

uniform vec4 u_PortalClip;
uniform vec4 u_PortalPlane;

uniform vec4 u_Generators;
#define u_ColorGen int(u_Generators[GEN_COLOR])

// light vector
uniform vec4 u_LightDirection;
uniform vec4 u_DirectedLight;
uniform vec4 u_AmbientLight;
uniform vec4 u_NormalScale;
uniform vec4 u_SpecularScale;

uniform vec4 u_LightType; // only x used

#if defined(USE_DYNAMIC_LIGHTS)
struct DynamicLight
{
	vec4 capsuleEnd;
	vec4 color_radius;
	vec4 position_type;
};

vec4 FetchDynamicLightData(uint offset)
{
	int u = int(offset) % int(u_DynamicLightTextureSizes_Cells_Indices_Lights.z);
	int v = int(offset) / int(u_DynamicLightTextureSizes_Cells_Indices_Lights.z);
	return texelFetch(u_DynamicLightsSampler, ivec2(u, v), 0);
}

DynamicLight GetDynamicLight(uint index)
{
	DynamicLight dl;
	dl.capsuleEnd = FetchDynamicLightData(index * 3u + 0u);
	dl.color_radius = FetchDynamicLightData(index * 3u + 1u);
	dl.position_type = FetchDynamicLightData(index * 3u + 2u);
	return dl;
}

uint GetDynamicLightIndicesOffset(vec3 position)
{
	vec3 local = u_DynamicLightGridOffset.xyz + position;
	uint cellX = min(uint(max(0, local.x / u_DynamicLightCellSize.x)), uint(u_DynamicLightGridSize.x) - 1u);
	uint cellY = min(uint(max(0, local.y / u_DynamicLightCellSize.y)), uint(u_DynamicLightGridSize.y) - 1u);
	uint cellZ = min(uint(max(0, local.z / u_DynamicLightCellSize.z)), uint(u_DynamicLightGridSize.z) - 1u);
	uint cellOffset = cellX + (cellY * uint(u_DynamicLightGridSize.x)) + (cellZ * uint(u_DynamicLightGridSize.x) * uint(u_DynamicLightGridSize.y));
	int u = int(cellOffset) % int(u_DynamicLightTextureSizes_Cells_Indices_Lights.x);
	int v = int(cellOffset) / int(u_DynamicLightTextureSizes_Cells_Indices_Lights.x);
	return texelFetch(u_DynamicLightCellsSampler, ivec2(u, v), 0).r;
}

uint GetDynamicLightIndicesData(uint offset)
{
	int u = int(offset) % int(u_DynamicLightTextureSizes_Cells_Indices_Lights.y);
	int v = int(offset) / int(u_DynamicLightTextureSizes_Cells_Indices_Lights.y);
	return texelFetch(u_DynamicLightIndicesSampler, ivec2(u, v), 0).r;
}
#endif // USE_DYNAMIC_LIGHTS

float Lambert(vec3 surfaceNormal, vec3 lightDir)
{
#if defined(USE_HALF_LAMBERT)
	return pow(dot(surfaceNormal, lightDir) * 0.5 + 0.5, 2);
#else
	return saturate(dot(surfaceNormal, lightDir));
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
	float scale = saturate(wsDelta / u_SoftSprite_Depth_UseAlpha.x);

	// Ignore existing alpha if the blend is additive, otherwise scale it.
	if (int(u_SoftSprite_Depth_UseAlpha.y) == 1)
	{
		alpha *= scale;
	}
	else
	{
		alpha = scale;		
	}
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
		diffuseLight = ToLinear(texture2D(u_LightMap, v_texcoord1).rgb);
	}
	else if (int(u_LightType.x) == LIGHT_VECTOR)
	{
		diffuseLight = u_AmbientLight.xyz + u_DirectedLight.xyz * Lambert(v_normal.xyz, u_LightDirection.xyz);
	}
	else // LIGHT_VERTEX or LIGHT_NONE
	{
		diffuseLight = vec3_splat(1.0);
	}

#if defined(USE_DYNAMIC_LIGHTS)
	if (int(u_DynamicLightNum.x) > 0 && (int(u_LightType.x) != LIGHT_NONE || u_ColorGen == CGEN_EXACT_VERTEX || u_ColorGen == CGEN_VERTEX))
	{
		uint indicesOffset = GetDynamicLightIndicesOffset(v_position);

		if (indicesOffset > 0u) // First index is reserved for empty cells.
		{
			uint numLights = GetDynamicLightIndicesData(indicesOffset);

#if 0
			// Heatmap
			if (numLights == 1u)
			{
				diffuseLight = vec3(0.0, 0.0, 1.0);
			}
			else if (numLights == 2u)
			{
				diffuseLight = vec3(0.0, 1.0, 1.0);
			}
			else if (numLights == 3u)
			{
				diffuseLight = vec3(0.0, 1.0, 0.0);
			}
			else if (numLights == 4u)
			{
				diffuseLight = vec3(1.0, 1.0, 0.0);
			}
			else if (numLights >= 5u)
			{
				diffuseLight = vec3(1.0, 0.0, 0.0);
			}
#else
			for (uint i = 0u; i < numLights; i++)
			{
				uint lightIndex = GetDynamicLightIndicesData(indicesOffset + 1u + i);
				DynamicLight light = GetDynamicLight(lightIndex);
				vec3 dir;

				if (int(light.position_type.w) == DLIGHT_POINT)
				{
					dir = light.position_type.xyz - v_position;
				}
				else // DLIGHT_CAPSULE
				{
					vec3 pointOnLine = ClosestPointOnLineSegment(light.position_type.xyz, light.capsuleEnd.xyz, v_position);
					dir = pointOnLine - v_position;
				}

				float attenuation = saturate(1.0 - length(dir) / light.color_radius.w);
				diffuseLight += light.color_radius.rgb * attenuation * Lambert(v_normal.xyz, normalize(dir));
			}
#endif
		}
	}
#endif // USE_DYNAMIC_LIGHTS

	gl_FragColor.rgb = ToGamma(diffuse.rgb * v_color0.rgb * diffuseLight);
}
