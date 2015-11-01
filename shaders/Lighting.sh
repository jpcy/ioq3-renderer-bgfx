#define EPSILON 0.00000001

#if defined(USE_PARALLAXMAP)
float SampleDepth(sampler2D normalMap, vec2 t)
{
  #if defined(SWIZZLE_NORMALMAP)
	return 1.0 - texture2D(normalMap, t).r;
  #else
	return 1.0 - texture2D(normalMap, t).a;
  #endif
}

float RayIntersectDisplaceMap(vec2 dp, vec2 ds, sampler2D normalMap)
{
	const int linearSearchSteps = 16;
	const int binarySearchSteps = 6;

	// current size of search window
	float size = 1.0 / float(linearSearchSteps);

	// current depth position
	float depth = 0.0;

	// best match found (starts with last position 1.0)
	float bestDepth = 1.0;

	// texture depth at best depth
	float texDepth = 0.0;

	float prevT = SampleDepth(normalMap, dp);
	float prevTexDepth = prevT;

	// search front to back for first point inside object
	for(int i = 0; i < linearSearchSteps - 1; ++i)
	{
		depth += size;
		
		float t = SampleDepth(normalMap, dp + ds * depth);
		
		if(bestDepth > 0.996)		// if no depth found yet
			if(depth >= t)
			{
				bestDepth = depth;	// store best depth
				texDepth = t;
				prevTexDepth = prevT;
			}
		prevT = t;
	}

	depth = bestDepth;

#if !defined (USE_RELIEFMAP)
	float div = 1.0 / (1.0 + (prevTexDepth - texDepth) * float(linearSearchSteps));
	bestDepth -= (depth - size - prevTexDepth) * div;
#else
	// recurse around first point (depth) for closest match
	for(int i = 0; i < binarySearchSteps; ++i)
	{
		size *= 0.5;

		float t = SampleDepth(normalMap, dp + ds * depth);
		
		if(depth >= t)
		{
			bestDepth = depth;
			depth -= 2.0 * size;
		}

		depth += size;
	}
#endif

	return bestDepth;
}
#endif

vec3 CalcDiffuse(vec3 diffuseAlbedo, vec3 N, vec3 L, vec3 E, float NE, float NL, float shininess)
{
  #if defined(USE_OREN_NAYAR) || defined(USE_TRIACE_OREN_NAYAR)
	float gamma = dot(E, L) - NE * NL;
	float B = 2.22222 + 0.1 * shininess;
		
    #if defined(USE_OREN_NAYAR)
	float A = 1.0 - 1.0 / (2.0 + 0.33 * shininess);
	gamma = clamp(gamma, 0.0, 1.0);
    #endif
	
    #if defined(USE_TRIACE_OREN_NAYAR)
	float A = 1.0 - 1.0 / (2.0 + 0.65 * shininess);

	if (gamma >= 0.0)
    #endif
	{
		B = max(B * max(NL, NE), EPSILON);
	}

	return diffuseAlbedo * (A + gamma / B);
  #else
	return diffuseAlbedo;
  #endif
}

vec3 EnvironmentBRDF(float gloss, float NE, vec3 specular)
{
  #if 1
	// from http://blog.selfshadow.com/publications/s2013-shading-course/lazarov/s2013_pbs_black_ops_2_notes.pdf
	vec4 t = vec4( 1.0/0.96, 0.475, (0.0275 - 0.25 * 0.04)/0.96,0.25 ) * gloss;
	t += vec4( 0.0, 0.0, (0.015 - 0.75 * 0.04)/0.96,0.75 );
	float a0 = t.x * min( t.y, exp2( -9.28 * NE ) ) + t.z;
	float a1 = t.w;
	return clamp( a0 + specular * ( a1 - a0 ), 0.0, 1.0 );
  #elif 0
	// from http://seblagarde.wordpress.com/2011/08/17/hello-world/
	return specular + CalcFresnel(NE) * clamp(vec3(gloss) - specular, 0.0, 1.0);
  #else
	// from http://advances.realtimerendering.com/s2011/Lazarov-Physically-Based-Lighting-in-Black-Ops%20%28Siggraph%202011%20Advances%20in%20Real-Time%20Rendering%20Course%29.pptx
	return mix(specular.rgb, vec3(1.0, 1.0, 1.0), CalcFresnel(NE) / (4.0 - 3.0 * gloss));
  #endif
}

float CalcBlinn(float NH, float shininess)
{
#if defined(USE_BLINN) || defined(USE_BLINN_FRESNEL)
	// Normalized Blinn-Phong
	float norm = shininess * 0.125    + 1.0;
#elif defined(USE_MCAULEY)
	// Cook-Torrance as done by Stephen McAuley
	// http://blog.selfshadow.com/publications/s2012-shading-course/mcauley/s2012_pbs_farcry3_notes_v2.pdf
	float norm = shininess * 0.25     + 0.125;
#elif defined(USE_GOTANDA)
	// Neumann-Neumann as done by Yoshiharu Gotanda
	// http://research.tri-ace.com/Data/s2012_beyond_CourseNotes.pdf
	float norm = shininess * 0.124858 + 0.269182;
#elif defined(USE_LAZAROV)
	// Cook-Torrance as done by Dimitar Lazarov
	// http://blog.selfshadow.com/publications/s2013-shading-course/lazarov/s2013_pbs_black_ops_2_notes.pdf
	float norm = shininess * 0.125    + 0.25;
#else
	float norm = 1.0;
#endif

#if 0
	// from http://seblagarde.wordpress.com/2012/06/03/spherical-gaussien-approximation-for-blinn-phong-phong-and-fresnel/
	float a = shininess + 0.775;
	return norm * exp(a * NH - a);
#else
	return norm * pow(NH, shininess);
#endif
}

float CalcGGX(float NH, float gloss)
{
	// from http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
	float a_sq = exp2(gloss * -13.0 + 1.0);
	float d = ((NH * NH) * (a_sq - 1.0) + 1.0);
	return a_sq / (d * d);
}

float CalcFresnel(float EH)
{
#if 1
	// From http://blog.selfshadow.com/publications/s2013-shading-course/lazarov/s2013_pbs_black_ops_2_notes.pdf
	// not accurate, but fast
	return exp2(-10.0 * EH);
#elif 0
	// From http://seblagarde.wordpress.com/2012/06/03/spherical-gaussien-approximation-for-blinn-phong-phong-and-fresnel/
	return exp2((-5.55473 * EH - 6.98316) * EH);
#elif 0
	float blend = 1.0 - EH;
	float blend2 = blend * blend;
	blend *= blend2 * blend2;
	
	return blend;
#else
	return pow(1.0 - EH, 5.0);
#endif
}

float CalcVisibility(float NH, float NL, float NE, float EH, float gloss)
{
#if defined(USE_GOTANDA)
	// Neumann-Neumann as done by Yoshiharu Gotanda
	// http://research.tri-ace.com/Data/s2012_beyond_CourseNotes.pdf
	return 1.0 / max(max(NL, NE), EPSILON);
#elif defined(USE_LAZAROV)
	// Cook-Torrance as done by Dimitar Lazarov
	// http://blog.selfshadow.com/publications/s2013-shading-course/lazarov/s2013_pbs_black_ops_2_notes.pdf
	float k = min(1.0, gloss + 0.545);
	return 1.0 / (k * (EH * EH - 1.0) + 1.0);
#elif defined(USE_GGX)
	float roughness = exp2(gloss * -6.5);

	// Modified from http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
	// NL, NE in numerator factored out from cook-torrance
	float k = roughness + 1.0;
	k *= k * 0.125;

	float k2 = 1.0 - k;
	
	float invGeo1 = NL * k2 + k;
	float invGeo2 = NE * k2 + k;

	return 1.0 / (invGeo1 * invGeo2);
#else
	return 1.0;
#endif
}

vec3 CalcSpecular(vec3 specular, float NH, float NL, float NE, float EH, float gloss, float shininess)
{
#if defined(USE_GGX)
	float distrib = CalcGGX(NH, gloss);
#else
	float distrib = CalcBlinn(NH, shininess);
#endif

#if defined(USE_BLINN)
	vec3 fSpecular = specular;
#else
	vec3 fSpecular = mix(specular, vec3(1.0, 1.0, 1.0), CalcFresnel(EH));
#endif

	float vis = CalcVisibility(NH, NL, NE, EH, gloss);

	return fSpecular * (distrib * vis);
}

float CalcLightAttenuation(float _point, float normDist)
{
	// zero light at 1.0, approximating q3 style
	// also don't attenuate directional light
	float attenuation = (0.5 * normDist - 1.5) * _point + 1.0;

	// clamp attenuation
	#if defined(NO_LIGHT_CLAMP)
	attenuation = max(attenuation, 0.0);
	#else
	attenuation = clamp(attenuation, 0.0, 1.0);
	#endif

	return attenuation;
}
