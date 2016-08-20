$input v_position, v_projPosition, v_texcoord0, v_texcoord1, v_normal, v_color0

#include <bgfx_shader.sh>
#include "Common.sh"
#include "SharedDefines.sh"

SAMPLER2D(u_DiffuseSampler, TU_DIFFUSE);
SAMPLER2D(u_DiffuseSampler2, TU_DIFFUSE2);
SAMPLER2D(u_LightSampler, TU_LIGHT);

#if defined(USE_ALPHA_TEST)
uniform vec4 u_AlphaTest; // only x used
#endif

#if defined(USE_SOFT_SPRITE)
SAMPLER2D(u_DepthSampler, TU_DEPTH);

uniform vec4 u_DepthRange;
uniform vec4 u_SoftSprite_Depth_UseAlpha; // only x and y used
#endif

#if defined(USE_DYNAMIC_LIGHTS)
USAMPLER2D(u_DynamicLightCellsSampler, TU_DYNAMIC_LIGHT_CELLS);
USAMPLER2D(u_DynamicLightIndicesSampler, TU_DYNAMIC_LIGHT_INDICES);
SAMPLER2D(u_DynamicLightsSampler, TU_DYNAMIC_LIGHTS);

uniform vec4 u_DynamicLightCellSize; // xyz is size
uniform vec4 u_DynamicLightGridOffset; // w not used
uniform vec4 u_DynamicLightGridSize; // w not used
uniform vec4 u_DynamicLight_Num_Intensity; // x is the number of dynamic lights, y is the intensity scale
uniform vec4 u_DynamicLightTextureSizes_Cells_Indices_Lights; // w not used
#endif

#if defined(USE_HDR)
uniform vec4 u_BloomEnabled; // only x used
#endif

uniform vec4 u_Animation_Enabled_Fraction; // only x and y used
uniform vec4 u_DiffuseRGBM; // only x used
uniform vec4 u_PortalClip;
uniform vec4 u_PortalPlane;
uniform vec4 u_ViewOrigin;

uniform vec4 u_Generators;
#define u_TexCoordGen int(u_Generators[GEN_TEXCOORD])
#define u_ColorGen int(u_Generators[GEN_COLOR])
#define u_AlphaGen int(u_Generators[GEN_ALPHA])

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

	vec2 texCoord0 = v_texcoord0;

	if (u_TexCoordGen == TCGEN_FRAGMENT)
	{
		texCoord0 = gl_FragCoord.xy * u_viewTexel.xy;
	}

	vec4 diffuse = texture2D(u_DiffuseSampler, texCoord0);

	if (int(u_DiffuseRGBM.x) == 1.0)
	{
		diffuse = vec4(DecodeRGBM(diffuse), 1.0);
	}

	if (int(u_Animation_Enabled_Fraction.x) == 1.0)
	{
		vec4 diffuse2 = texture2D(u_DiffuseSampler2, texCoord0);
		diffuse = mix(diffuse, diffuse2, u_Animation_Enabled_Fraction.y);
	}

	float alpha;

	if (u_AlphaGen == AGEN_WATER)
	{
		const float minReflectivity = 0.1;
		vec3 viewDir = normalize(u_ViewOrigin.xyz - v_position);
		alpha = minReflectivity + (1.0 - minReflectivity) * pow(1.0 - dot(v_normal.xyz, viewDir), 5);
	}
	else
	{
		alpha = diffuse.a * v_color0.a;;
	}

#if defined(USE_SOFT_SPRITE)
	// Normalized linear depths.
	float sceneDepth = texture2D(u_DepthSampler, gl_FragCoord.xy * u_viewTexel.xy).r;

	// GL uses -1 to 1 NDC. D3D uses 0 to 1.
#if BGFX_SHADER_LANGUAGE_HLSL
	float fragmentDepth = ToLinearDepth(v_projPosition.z / v_projPosition.w, u_DepthRange.z, u_DepthRange.w);
#else
	float fragmentDepth = ToLinearDepth(v_projPosition.z / v_projPosition.w * 0.5 + 0.5, u_DepthRange.z, u_DepthRange.w);
#endif

	float spriteDepth = u_SoftSprite_Depth_UseAlpha.x;

	// Depth change in worldspace.
	float wsDelta = (sceneDepth - fragmentDepth) * (u_DepthRange.w - u_DepthRange.z);
	float scale = saturate(abs(wsDelta / spriteDepth));

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

	vec3 vertexColor = v_color0.rgb;
	vec3 diffuseLight = vec3_splat(1.0);
	int lightType = int(u_LightType.x);

	if (lightType == LIGHT_MAP)
	{
		diffuseLight = DecodeRGBM(texture2D(u_LightSampler, v_texcoord1));
	}
	else if (lightType == LIGHT_VECTOR)
	{
		diffuseLight = u_AmbientLight.xyz + u_DirectedLight.xyz * Lambert(v_normal.xyz, u_LightDirection.xyz);
	}

#if defined(USE_DYNAMIC_LIGHTS)
	// Treat vertex colors as diffuse light so dynamic lighting is applied correctly.
	if (lightType == LIGHT_NONE && (u_ColorGen == CGEN_EXACT_VERTEX || u_ColorGen == CGEN_VERTEX))
	{
		diffuseLight = vertexColor;
		vertexColor = vec3_splat(1.0);
	}

	if (int(u_DynamicLight_Num_Intensity.x) > 0)
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

				float inverseNormalizedDistance = max(1.0 - length(dir) / light.color_radius.w, 0.0); // 1 at center, 0 at edge
				float attenuation = min(2.0 * inverseNormalizedDistance, 1.0); // 1 at top half, lerp between 1 and 0 at bottom half
				diffuseLight += light.color_radius.rgb * attenuation * Lambert(v_normal.xyz, normalize(dir)) * u_DynamicLight_Num_Intensity.y;
			}
#endif
		}
	}
#endif // USE_DYNAMIC_LIGHTS

	vec4 fragColor = vec4(diffuse.rgb * vertexColor * diffuseLight, alpha);

#if defined(USE_HDR)
	gl_FragData[0] = fragColor;

	if (int(u_BloomEnabled.x) != 0)
	{
		gl_FragData[1] = fragColor;
	}
	else
	{
		gl_FragData[1] = vec4(0.0, 0.0, 0.0, fragColor.a);
	}
#else
	gl_FragColor = fragColor;
#endif // USE_HDR
}
