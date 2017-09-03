$input v_position, v_projPosition, v_shadowPosition, v_texcoord0, v_texcoord1, v_normal, v_color0

#include <bgfx_shader.sh>
#include "Common.sh"
#include "SharedDefines.sh"
#define USE_DYNAMIC_LIGHTS
#include "DynamicLight.sh"
#include "PortalClip.sh"
#include "SunLight.sh"

SAMPLER2D(u_DiffuseSampler, 0); // TU_DIFFUSE
SAMPLER2D(u_LightSampler, 2); // TU_LIGHT

uniform vec4 u_Bloom_Enabled_Write_Scale;
#define u_BloomEnabled int(u_Bloom_Enabled_Write_Scale.x)
#define u_BloomWrite int(u_Bloom_Enabled_Write_Scale.y)

uniform vec4 u_RenderMode; // only x used

uniform vec4 u_Generators;
#define u_TexCoordGen int(u_Generators[GEN_TEXCOORD])


// https://www.shadertoy.com/view/4tyGWK
// http://www.iquilezles.org/www/articles/texturerepetition/texturerepetition.htm
// Modification by huwb. Original shader by iq: https://www.shadertoy.com/view/lt2GDd
// Created by inigo quilez - iq/2015
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

// utilities for randomizing uvs
vec4 hash4( vec2 p ) { return fract(sin(vec4( 1.0+dot(p,vec2(37.0,17.0)), 2.0+dot(p,vec2(11.0,47.0)), 3.0+dot(p,vec2(41.0,29.0)), 4.0+dot(p,vec2(23.0,31.0))))*103.); }
vec2 transformUVs( vec2 iuvCorner, vec2 uv )
{
    // random in [0,1]^4
	vec4 tx = hash4( iuvCorner );
    // scale component is +/-1 to mirror
    tx.zw = sign( tx.zw - 0.5 );
    // random scale and offset
	return tx.zw * uv + tx.xy;
}

// here is a derivative-free version of the 4 samples algorithm from iq
vec4 textureNoTile_4weights(vec2 uv)
{
    // compute per-tile integral and fractional uvs.
    // flip uvs for 'odd' tiles to make sure tex samples are coherent
    vec2 fuv = mod( uv, 2. ), iuv = uv - fuv;
    vec3 BL_one = vec3(0.,0.,1.); // xy = bot left coords, z = 1
    if( fuv.x >= 1. ) fuv.x = 2.-fuv.x, BL_one.x = 2.;
    if( fuv.y >= 1. ) fuv.y = 2.-fuv.y, BL_one.y = 2.;
    
    // smoothstep for fun and to limit blend overlap
    vec2 b = smoothstep(0.25,0.75,fuv);
    
    // fetch and blend
    vec4 res = mix(
        		mix( texture2D( u_DiffuseSampler, transformUVs( iuv + BL_one.xy, uv ) ), 
                     texture2D( u_DiffuseSampler, transformUVs( iuv + BL_one.zy, uv ) ), b.x ), 
                mix( texture2D( u_DiffuseSampler, transformUVs( iuv + BL_one.xz, uv ) ),
                     texture2D( u_DiffuseSampler, transformUVs( iuv + BL_one.zz, uv ) ), b.x),
        		b.y );
  
    return res;
}

void main()
{
	if (PortalClipped(v_position))
		discard;

	vec2 texCoord0 = v_texcoord0;

	if (u_TexCoordGen == TCGEN_FRAGMENT)
	{
		texCoord0 = gl_FragCoord.xy * u_viewTexel.xy;
	}

	vec4 diffuse = textureNoTile_4weights(texCoord0);
	diffuse.rgb = ToLinear(diffuse.rgb);
	float alpha = diffuse.a * v_color0.a;;
	vec3 vertexColor = v_color0.rgb;
	vec3 diffuseLight = ToLinear(texture2D(u_LightSampler, v_texcoord1).rgb);
	diffuseLight += CalculateDynamicLight(v_position, v_normal.xyz);
#if defined(USE_SUN_LIGHT)
	diffuseLight += CalculateSunLight(v_position, v_normal.xyz, v_shadowPosition);
#endif
	vec4 fragColor = vec4(ToGamma(diffuse.rgb * vertexColor * diffuseLight), alpha);
	if (int(u_RenderMode.x) == RENDER_MODE_LIGHTMAP)
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
