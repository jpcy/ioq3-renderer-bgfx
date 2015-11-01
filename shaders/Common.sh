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
