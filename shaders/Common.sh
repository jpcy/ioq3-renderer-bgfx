vec4 ApplyDepthRange(vec4 v, float offset, float scale)
{
	float z = v.z / v.w;

	// GL uses -1 to 1 NDC. D3D uses 0 to 1.
#if BGFX_SHADER_LANGUAGE_HLSL
	z = z * 2.0 - 1.0;
#endif

	z = offset + z * scale;

#if BGFX_SHADER_LANGUAGE_HLSL
	z = z * 0.5 + 0.5;
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
