#include "SharedDefines.sh"

vec4 ApplyDepthRange(vec4 v, float offset, float scale)
{
	float z = v.z / v.w;

	// GL uses -1 to 1 NDC. D3D uses 0 to 1.
#if BGFX_SHADER_LANGUAGE_GLSL
	z = z * 0.5 + 0.5;
#endif

	z = offset + z * scale;

#if BGFX_SHADER_LANGUAGE_GLSL
	z = z * 2.0 - 1.0;
#endif

	return vec4(v.x, v.y, z * v.w, v.w);
}

float CalcFog(vec3 position, vec4 fogDepth, vec4 fogDistance, float fogEyeT)
{
	float s = dot(vec4(position, 1.0), fogDistance) * 8.0;
	float t = dot(vec4(position, 1.0), fogDepth);

	float eyeOutside = float(fogEyeT < 0.0);
	float fogged = float(t >= eyeOutside);

	t += 1e-6;
	t *= fogged / (t - fogEyeT * eyeOutside);

	return s * t;
}

float Lambert(vec3 surfaceNormal, vec3 lightDir)
{
#if defined(USE_HALF_LAMBERT)
	return pow(dot(surfaceNormal, lightDir) * 0.5 + 0.5, 2);
#else
	return saturate(dot(surfaceNormal, lightDir));
#endif
}

float ToLinearDepth(float z, float near, float far)
{
	float linearDepth = 2.0 * near * far / (z * (far - near) - (far + near));
	linearDepth = (-linearDepth - near) / (far - near);
	return linearDepth;
}

vec3 ToGamma(vec3 v)
{
	return pow(abs(v), vec3_splat(1.0/2.2));
}

vec4 ToGamma(vec4 v)
{
	return vec4(ToGamma(v.xyz), v.w);
}

vec3 ToLinear(vec3 v)
{
	return pow(abs(v), vec3_splat(2.2));
}

vec4 ToLinear(vec4 v)
{
	return vec4(ToLinear(v.xyz), v.w);
}

vec3 DecodeRGBM(vec4 v)
{
	return v.xyz * v.a * RGBM_MAX_RANGE;
}