#if defined(USE_DYNAMIC_LIGHTS)
USAMPLER2D(u_DynamicLightCellsSampler, 4); // TU_DYNAMIC_LIGHT_CELLS
USAMPLER2D(u_DynamicLightIndicesSampler, 5); // TU_DYNAMIC_LIGHT_INDICES
SAMPLER2D(u_DynamicLightsSampler, 6); // TU_DYNAMIC_LIGHTS

uniform vec4 u_DynamicLightCellSize; // xyz is size
uniform vec4 u_DynamicLightGridOffset; // w not used
uniform vec4 u_DynamicLightGridSize; // w not used
uniform vec4 u_DynamicLight_Num_Intensity; // x is the number of dynamic lights, y is the intensity scale
uniform vec4 u_DynamicLightTextureSizes_Cells_Indices_Lights; // w not used

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

vec3 CalculateDynamicLight(vec3 position, vec3 normal)
{
	vec3 diffuseLight = vec3_splat(0.0);

	if (int(u_DynamicLight_Num_Intensity.x) > 0)
	{
		uint indicesOffset = GetDynamicLightIndicesOffset(position);

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
					dir = light.position_type.xyz - position;
				}
				else // DLIGHT_CAPSULE
				{
					vec3 pointOnLine = ClosestPointOnLineSegment(light.position_type.xyz, light.capsuleEnd.xyz, position);
					dir = pointOnLine - position;
				}

				float inverseNormalizedDistance = max(1.0 - length(dir) / light.color_radius.w, 0.0); // 1 at center, 0 at edge
				float attenuation = min(2.0 * inverseNormalizedDistance, 1.0); // 1 at top half, lerp between 1 and 0 at bottom half
				diffuseLight += light.color_radius.rgb * attenuation * Lambert(normal, normalize(dir)) * u_DynamicLight_Num_Intensity.y;
			}
#endif
		}
	}

	return diffuseLight;
}
#endif
