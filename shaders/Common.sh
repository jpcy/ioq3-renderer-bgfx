#if BGFX_SHADER_LANGUAGE_HLSL
struct BgfxUSampler2D
{
	Texture2D<uvec4> m_texture;
};

#define USAMPLER2D(_name, _reg) \
	uniform Texture2D<uvec4> _name ## Texture : register(t[_reg]); \
	static BgfxUSampler2D _name = { _name ## Texture }

vec4 texelFetch(BgfxSampler2D _sampler, ivec2 _coord, int _level)
{
	return _sampler.m_texture.Load(ivec3(_coord, _level));
}

uvec4 texelFetch(BgfxUSampler2D _sampler, ivec2 _coord, int _level)
{
	return _sampler.m_texture.Load(ivec3(_coord, _level));
}

#else
#define USAMPLER2D(_name, _reg) uniform usampler2D _name
#endif

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
