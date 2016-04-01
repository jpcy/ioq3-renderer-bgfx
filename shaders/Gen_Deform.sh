#include "SharedDefines.sh"

uniform vec4 u_NumDeforms; // only x used
uniform vec4 u_DeformMoveDirs[MAX_DEFORMS]; // only xyz used
uniform vec4 u_Deform_Gen_Wave_Base_Amplitude[MAX_DEFORMS];
uniform vec4 u_Deform_Frequency_Phase_Spread[MAX_DEFORMS];

void CalculateDeformSingle(inout vec3 pos, vec3 normal, const vec2 st, float time, int gen, int wave, float base, float amplitude, float freq, float phase, float spread, vec4 moveDir)
{
	if (gen == DGEN_BULGE)
	{
		phase *= st.x;
	}
	else if (gen == DGEN_WAVE)
	{
		phase += dot(pos.xyz, vec3(spread, spread, spread));
	}

	float value = phase + (time * freq);
	float func;

	if (wave == DGEN_WAVE_SIN)
	{
		func = sin(value * 2.0 * M_PI);
	}
	else if (wave == DGEN_WAVE_SQUARE)
	{
		func = sign(fract(0.5 - value));
	}
	else if (wave == DGEN_WAVE_TRIANGLE)
	{
		func = abs(fract(value + 0.75) - 0.5) * 4.0 - 1.0;
	}
	else if (wave == DGEN_WAVE_SAWTOOTH)
	{
		func = fract(value);
	}
	else if (wave == DGEN_WAVE_INVERSE_SAWTOOTH)
	{
		func = (1.0 - fract(value));
	}
	else
	{
		func = sin(value);
	}

	if (gen == DGEN_MOVE)
	{
		pos = pos + moveDir.xyz * (base + func * amplitude);
	}
	else
	{
		pos = pos + normal * (base + func * amplitude);
	}
}

void CalculateDeform(inout vec3 pos, vec3 normal, const vec2 st, float time)
{
	for (int i = 0; i < int(u_NumDeforms.x); i++)
	{
		CalculateDeformSingle(pos, normal, st, time, int(u_Deform_Gen_Wave_Base_Amplitude[i].x), int(u_Deform_Gen_Wave_Base_Amplitude[i].y), u_Deform_Gen_Wave_Base_Amplitude[i].z, u_Deform_Gen_Wave_Base_Amplitude[i].w, u_Deform_Frequency_Phase_Spread[i].x, u_Deform_Frequency_Phase_Spread[i].y, u_Deform_Frequency_Phase_Spread[i].z, u_DeformMoveDirs[i]);
	}
}
