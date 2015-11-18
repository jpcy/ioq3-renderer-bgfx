uniform vec4 u_Generators;
#define u_TCGen0 u_Generators[GEN_TEXCOORD]
#define u_ColorGen u_Generators[GEN_COLOR]
#define u_AlphaGen u_Generators[GEN_ALPHA]

// tcmod
uniform vec4   u_DiffuseTexMatrix;
uniform vec4   u_DiffuseTexOffTurb;

uniform vec4   u_LocalViewOrigin;

uniform vec4   u_TCGen0Vector0;
uniform vec4   u_TCGen0Vector1;

uniform vec4 u_NumDeforms; // only x used
uniform vec4 u_DeformTypeBaseAmplitudeFrequency[MAX_DEFORMS];
uniform vec4 u_DeformPhaseSpread[MAX_DEFORMS];

uniform vec4   u_AmbientLight;
uniform vec4   u_DirectedLight;
uniform vec4   u_ModelLightDir;
uniform vec4  u_PortalRange;

vec3 CalculateDeform(const vec3 pos, const vec3 normal, const vec2 st, float time, float type, float base, float amplitude, float freq, float phase, float spread)
{
	if (type == DGEN_BULGE)
	{
		phase *= st.x;
	}
	else // if (type <= DGEN_WAVE_INVERSE_SAWTOOTH)
	{
		phase += dot(pos.xyz, vec3(spread, spread, spread));
	}

	float value = phase + (time * freq);
	float func;

	if (type == DGEN_WAVE_SIN)
	{
		func = sin(value * 2.0 * M_PI);
	}
	else if (type == DGEN_WAVE_SQUARE)
	{
		func = sign(fract(0.5 - value));
	}
	else if (type == DGEN_WAVE_TRIANGLE)
	{
		func = abs(fract(value + 0.75) - 0.5) * 4.0 - 1.0;
	}
	else if (type == DGEN_WAVE_SAWTOOTH)
	{
		func = fract(value);
	}
	else if (type == DGEN_WAVE_INVERSE_SAWTOOTH)
	{
		func = (1.0 - fract(value));
	}
	else // if (type == DGEN_BULGE)
	{
		func = sin(value);
	}

	return pos + normal * (base + func * amplitude);
}

vec3 DeformPosition(const vec3 pos, const vec3 normal, const vec2 st, float time)
{
	vec3 deformedPos = pos;

	for (int i = 0; i < u_NumDeforms.x; i++)
	{
		deformedPos = CalculateDeform(deformedPos, normal, st, time, u_DeformTypeBaseAmplitudeFrequency[i].x, u_DeformTypeBaseAmplitudeFrequency[i].y, u_DeformTypeBaseAmplitudeFrequency[i].z, u_DeformTypeBaseAmplitudeFrequency[i].w, u_DeformPhaseSpread[i].x, u_DeformPhaseSpread[i].y);
	}

	return deformedPos;
}

vec2 GenTexCoords(vec3 position, vec3 normal, vec2 texCoord1, vec2 texCoord2)
{
	vec2 tex = texCoord1;

	if (u_TCGen0 == TCGEN_LIGHTMAP)
	{
		tex = texCoord2;
	}
	else if (u_TCGen0 == TCGEN_ENVIRONMENT_MAPPED)
	{
		vec3 viewer = normalize(u_LocalViewOrigin.xyz - position);
		vec2 ref = reflect(viewer, normal).yz;
		tex.x = ref.x * -0.5 + 0.5;
		tex.y = ref.y *  0.5 + 0.5;
	}
	else if (u_TCGen0 == TCGEN_VECTOR)
	{
		tex = vec2(dot(position, u_TCGen0Vector0.xyz), dot(position, u_TCGen0Vector1.xyz));
	}
	
	return tex;
}

vec2 ModTexCoords(vec2 st, vec3 position, vec4 texMatrix, vec4 offTurb)
{
	float amplitude = offTurb.z;
	float phase = offTurb.w * 2.0 * M_PI;
	vec2 st2;
	st2.x = st.x * texMatrix.x + (st.y * texMatrix.z + offTurb.x);
	st2.y = st.x * texMatrix.y + (st.y * texMatrix.w + offTurb.y);

	vec2 offsetPos = vec2(position.x + position.z, position.y);
	
	vec2 texOffset = sin(offsetPos * (2.0 * M_PI / 1024.0) + vec2(phase, phase));
	
	return st2 + texOffset * amplitude;	
}

vec4 CalcColor(vec4 vertColor, vec4 baseColor, vec4 colorAttrib, vec3 position, vec3 normal)
{
	vec4 color = vertColor * colorAttrib + baseColor;
	
	if (u_ColorGen == CGEN_LIGHTING_DIFFUSE)
	{
		float incoming = clamp(dot(normal, u_ModelLightDir.xyz), 0.0, 1.0);

		color.rgb = clamp(u_DirectedLight.xyz * incoming + u_AmbientLight.xyz, 0.0, 1.0);
	}
	
	vec3 viewer = u_LocalViewOrigin.xyz - position;

	if (u_AlphaGen == AGEN_LIGHTING_SPECULAR)
	{
		vec3 lightDir = normalize(vec3(-960.0, 1980.0, 96.0) - position);
		vec3 reflected = -reflect(lightDir, normal);
		
		color.a = clamp(dot(reflected, normalize(viewer)), 0.0, 1.0);
		color.a *= color.a;
		color.a *= color.a;
	}
	else if (u_AlphaGen == AGEN_PORTAL)
	{
		color.a = clamp(length(viewer) / u_PortalRange.x, 0.0, 1.0);
	}
	
	return color;
}
