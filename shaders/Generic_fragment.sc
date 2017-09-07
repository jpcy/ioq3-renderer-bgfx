$input v_position, v_projPosition, v_shadowPosition, v_texcoord0, v_texcoord1, v_normal, v_color0

#include <bgfx_shader.sh>
#include "Common.sh"
#include "SharedDefines.sh"
#include "AlphaTest.sh"
#include "DynamicLight.sh"
#include "PortalClip.sh"
#include "SunLight.sh"

SAMPLER2D(u_DiffuseSampler, 0); // TU_DIFFUSE
SAMPLER2D(u_DiffuseSampler2, 1); // TU_DIFFUSE2
SAMPLER2D(u_LightSampler, 2); // TU_LIGHT

#if defined(USE_SOFT_SPRITE)
SAMPLER2D(u_DepthSampler, 3); // TU_DEPTH

uniform vec4 u_DepthRange;
uniform vec4 u_SoftSprite_Depth_UseAlpha; // only x and y used
#endif

uniform vec4 u_Bloom_Enabled_Write_Scale;
#define u_BloomEnabled int(u_Bloom_Enabled_Write_Scale.x)
#define u_BloomWrite int(u_Bloom_Enabled_Write_Scale.y)

uniform vec4 u_Animation_Enabled_Fraction; // only x and y used
uniform vec4 u_RenderMode; // only x used
uniform vec4 u_ViewOrigin;

uniform vec4 u_Generators;
#define u_TexCoordGen int(u_Generators[GEN_TEXCOORD])
#define u_ColorGen int(u_Generators[GEN_COLOR])
#define u_AlphaGen int(u_Generators[GEN_ALPHA])

// light vector
uniform vec4 u_LightDirection;
uniform vec4 u_DirectedLight;
uniform vec4 u_AmbientLight;

uniform vec4 u_LightType; // only x used

void main()
{
	if (PortalClipped(v_position))
		discard;

	vec2 texCoord0 = v_texcoord0;

	if (u_TexCoordGen == TCGEN_FRAGMENT)
	{
		texCoord0 = gl_FragCoord.xy * u_viewTexel.xy;
	}

	vec4 diffuse = texture2D(u_DiffuseSampler, texCoord0);

	if (int(u_Animation_Enabled_Fraction.x) != 0)
	{
		vec4 diffuse2 = texture2D(u_DiffuseSampler2, texCoord0);
		diffuse = mix(diffuse, diffuse2, u_Animation_Enabled_Fraction.y);
	}

	diffuse.rgb = ToLinear(diffuse.rgb);
	float alpha;

	if (u_AlphaGen == AGEN_WATER)
	{
		const float minReflectivity = 0.1;
		vec3 viewDir = normalize(u_ViewOrigin.xyz - v_position);
		alpha = minReflectivity + (1.0 - minReflectivity) * pow(1.0 - dot(v_normal.xyz, viewDir), 5);
	}
	else
	{
		alpha = diffuse.a * v_color0.a;
	}

#if defined(USE_SOFT_SPRITE)
	// Normalized linear depths.
	float sceneDepth = ToLinearDepth(texture2D(u_DepthSampler, gl_FragCoord.xy * u_viewTexel.xy).r, u_DepthRange.z, u_DepthRange.w);

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
	if (!AlphaTestPassed(alpha))
		discard;
#endif

	vec3 vertexColor = v_color0.rgb;
	vec3 diffuseLight = vec3_splat(1.0);
	int lightType = int(u_LightType.x);

	if (lightType == LIGHT_MAP)
	{
		diffuseLight = ToLinear(texture2D(u_LightSampler, v_texcoord1).rgb);
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

	diffuseLight += CalculateDynamicLight(v_position, v_normal.xyz);
#endif // USE_DYNAMIC_LIGHTS

#if defined(USE_SUN_LIGHT)
	diffuseLight += CalculateSunLight(v_position, v_normal.xyz, v_shadowPosition);
#endif

	vec4 fragColor = vec4(ToGamma(diffuse.rgb * vertexColor * diffuseLight), alpha);

	int renderMode = int(u_RenderMode.x);

	if (renderMode == RENDER_MODE_LIT && lightType != LIGHT_MAP)
	{
		fragColor = vec4(0, 0, 0, alpha);
	}
	else if (renderMode == RENDER_MODE_LIGHTMAP && lightType == LIGHT_MAP)
	{
		fragColor = vec4(texture2D(u_LightSampler, v_texcoord1).rgb, alpha);
	}

	gl_FragData[0] = fragColor;

	if (u_BloomEnabled != 0)
	{
		if (u_BloomWrite != 0)
		{
			gl_FragData[1] = fragColor;
		}
		else
		{
			gl_FragData[1] = vec4(0.0, 0.0, 0.0, fragColor.a);
		}
	}
}
