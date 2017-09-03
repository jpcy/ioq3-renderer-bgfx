#if BGFX_SHADER_TYPE_VERTEX
uniform vec4 u_ShadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias;
uniform mat4 u_LightModelViewProj; 
#define u_ShadowMapNormalBias u_ShadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias.z
#else // fragment
#if defined(USE_SUN_LIGHT)
SAMPLER2DSHADOW(u_ShadowMapSampler, 7); // TU_SHADOWMAP

uniform vec4 u_SunLightColor;
uniform vec4 u_SunLightDir;
uniform vec4 u_ShadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias;
#define u_ShadowMapTexelSize u_ShadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias.x
#define u_ShadowMapDepthBias u_ShadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias.y
#define u_ShadowMapSlopeScaleDepthBias u_ShadowMap_TexelSize_DepthBias_NormalBias_SlopeScaleDepthBias.w

vec3 CalculateSunLight(vec3 position, vec3 normal, vec4 shadowPosition)
{
	if (shadowPosition.w <= 0.0)
		return vec3_splat(0.0);

	vec3 lsPosition = shadowPosition.xyz / shadowPosition.w;
	lsPosition.x = lsPosition.x * 0.5 + 0.5;
	lsPosition.y = lsPosition.y * 0.5 + 0.5;
#if BGFX_SHADER_LANGUAGE_HLSL
	lsPosition.y = 1.0 - lsPosition.y;
#else
	lsPosition.z = lsPosition.z * 0.5 + 0.5;
#endif
	float bias = u_ShadowMapDepthBias + u_ShadowMapSlopeScaleDepthBias * tan(acos(saturate(dot(normal, -u_SunLightDir.xyz))));
	float visibility = 0.0;
	for (int x = -2; x <= 2; x++)
	{
		for (int y = -2; y <= 2; y++)
		{
			vec2 offset = vec2(float(x) * u_ShadowMapTexelSize, float(y) * u_ShadowMapTexelSize);
#if BGFX_SHADER_LANGUAGE_HLSL
			visibility += shadow2D(u_ShadowMapSampler, vec3(lsPosition.xy + offset, lsPosition.z - bias));
#else
			// FIXME: glsl optimizer bug, correctly converts shadow2D to texture but tries to swizzle float
			visibility += texture(u_ShadowMapSampler, vec3(lsPosition.xy + offset, lsPosition.z - bias));
#endif
		}
	}
	visibility /= 25.0;
	return u_SunLightColor.rgb * visibility;
}
#endif
#endif
