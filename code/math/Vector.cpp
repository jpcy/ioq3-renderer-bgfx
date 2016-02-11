/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2015 Jonathan Young

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "Math.h"

namespace math {

const vec2 vec2::empty;
const vec3 vec3::empty;
const vec4 vec4::empty;

const vec4 vec4::black(0.0f, 0.0f, 0.0f, 1.0f);
const vec4 vec4::red(1.0f, 0.0f, 0.0f, 1.0f);
const vec4 vec4::green(0.0f, 1.0f, 0.0f, 1.0f);
const vec4 vec4::yellow(1.0f, 1.0f, 0.0f, 1.0f);
const vec4 vec4::blue(0.0f, 0.0f, 1.0f, 1.0f);
const vec4 vec4::magenta(1.0f, 0.0f, 1.0f, 1.0f);
const vec4 vec4::cyan(0.0f, 1.0f, 1.0f, 1.0f);
const vec4 vec4::white(1.0f, 1.0f, 1.0f, 1.0f);
const vec4 vec4::lightGrey(0.75f, 0.75f, 0.75f, 1.0f);
const vec4 vec4::mediumGrey(0.5f, 0.5f, 0.5f, 1.0f);
const vec4 vec4::darkGrey(0.25f, 0.25f, 0.25f, 1.0f);

float vec3::dotProduct(const vec3 &v1, const vec3 &v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

vec3 vec3::crossProduct(const vec3 &v1, const vec3 &v2)
{
	return vec3(v1.y*v2.z - v1.z*v2.y, v1.z*v2.x - v1.x*v2.z, v1.x*v2.y - v1.y*v2.x);
}

vec3 vec3::lerp(const vec3 &from, const vec3 &to, float fraction)
{
	return vec3(math::Lerp(from.x, to.x, fraction), math::Lerp(from.y, to.y, fraction), math::Lerp(from.z, to.z, fraction));
}

float vec3::distance(const vec3 &v1, const vec3 &v2)
{
	return (v2 - v1).length();
}

float vec3::distanceSquared(const vec3 &v1, const vec3 &v2)
{
	return (v2 - v1).lengthSquared();
}

vec3 vec3::anglesSubtract(const vec3 &v1, const vec3 &v2)
{
	return vec3(math::AngleSubtract(v1[0], v2[0]), math::AngleSubtract(v1[1], v2[1]), math::AngleSubtract(v1[2], v2[2]));
}

float vec3::length() const
{
	return sqrt(x*x + y*y + z*z);
}

float vec3::lengthSquared() const
{
	return x*x + y*y + z*z;
}

vec3 vec3::absolute() const
{
	return vec3(fabs(x), fabs(y), fabs(z));
}

vec3 vec3::normal() const
{
	float l = length();

	if (l)
	{
		float il = 1/l;
		return vec3(x * il, y * il, z * il);
	}

	return vec3();
}

vec3 vec3::perpendicular() const
{
	int	pos = 0;
	float minelem = 1.0f;

	// Find the smallest magnitude axially aligned vector.
	for (int i = 0; i < 3; i++)
	{
		if (fabs((&x)[i]) < minelem)
		{
			pos = i;
			minelem = fabs((&x)[i]);
		}
	}

	vec3 temp;
	temp[pos] = 1.0f;

	return projectOnPlane(temp).normal();
}

void vec3::toNormalVectors(vec3 *right, vec3 *up) const
{
	assert(right);
	assert(up);

	// This rotate and negate guarantees a vector is not colinear with the original.
	*right = vec3(z, -x, y);
	const float d = vec3::dotProduct(*right, *this);
	*right = (*right + *this * -d).normal();
	*up = vec3::crossProduct(*right, *this);
}

vec3 vec3::toAngles() const
{
	float forward, yaw, pitch;

	if (y == 0 && x == 0)
	{
		yaw = 0;
		if (z > 0)
		{
			pitch = 90;
		}
		else
		{
			pitch = 270;
		}
	}
	else
	{
		if (x)
		{
			yaw = atan2(y, x) * 180 / M_PI;
		}
		else if (y > 0)
		{
			yaw = 90;
		}
		else
		{
			yaw = 270;
		}

		if (yaw < 0)
		{
			yaw += 360;
		}

		forward = sqrt(x * x + y * y);
		pitch = atan2(z, forward) * 180 / M_PI;

		if (pitch < 0)
		{
			pitch += 360;
		}
	}

	return vec3(-pitch, yaw, 0);
}

void vec3::toAngleVectors(vec3 *forward, vec3 *right, vec3 *up) const
{
	float angle;
	static float sr, sp, sy, cr, cp, cy; // static to help MS compiler fp bugs

	angle = (&x)[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = (&x)[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = (&x)[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	if (forward)
	{
		(*forward)[0] = cp*cy;
		(*forward)[1] = cp*sy;
		(*forward)[2] = -sp;
	}

	if (right)
	{
		(*right)[0] = (-1*sr*sp*cy+-1*cr*-sy);
		(*right)[1] = (-1*sr*sp*sy+-1*cr*cy);
		(*right)[2] = -1*sr*cp;
	}

	if (up)
	{
		(*up)[0] = (cr*sp*cy+-sr*-sy);
		(*up)[1] = (cr*sp*sy+-sr*cy);
		(*up)[2] = cr*cp;
	}
}

vec3 vec3::rotated(const vec3 &direction, float degrees) const
{
	float cos_ia = degrees * M_PI / 180.0f;
	float sin_a = sin(cos_ia);
	float cos_a = cos(cos_ia);
	cos_ia = 1.0F - cos_a;

	float i_i_ia = direction[0] * direction[0] * cos_ia;
	float j_j_ia = direction[1] * direction[1] * cos_ia;
	float k_k_ia = direction[2] * direction[2] * cos_ia;
	float i_j_ia = direction[0] * direction[1] * cos_ia;
	float i_k_ia = direction[0] * direction[2] * cos_ia;
	float j_k_ia = direction[1] * direction[2] * cos_ia;

	float a_sin = direction[0] * sin_a;
	float b_sin = direction[1] * sin_a;
	float c_sin = direction[2] * sin_a;

	float rot[3][3];
	rot[0][0] = i_i_ia + cos_a;
	rot[0][1] = i_j_ia - c_sin;
	rot[0][2] = i_k_ia + b_sin;
	rot[1][0] = i_j_ia + c_sin;
	rot[1][1] = j_j_ia + cos_a;
	rot[1][2] = j_k_ia - a_sin;
	rot[2][0] = i_k_ia - b_sin;
	rot[2][1] = j_k_ia + a_sin;
	rot[2][2] = k_k_ia + cos_a;

	vec3 dst;
	dst[0] = x * rot[0][0] + y * rot[0][1] + z * rot[0][2];
	dst[1] = x * rot[1][0] + y * rot[1][1] + z * rot[1][2];
	dst[2] = x * rot[2][0] + y * rot[2][1] + z * rot[2][2];

	return dst;
}

static void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
}

// This is not implemented very well...
vec3 vec3::rotatedAroundDirection(vec3 direction, float degrees) const
{
	vec3 vf(direction);
	vec3 vr(direction.perpendicular());
	vec3 vup(vec3::crossProduct(vr, vf));

	float m[3][3];
	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	float im[3][3];
	memcpy(im, m, sizeof(im));
	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	float zrot[3][3];
	memset(zrot, 0, sizeof(zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0f;
	const float rad = DEG2RAD(degrees);
	zrot[0][0] = cos( rad );
	zrot[0][1] = sin( rad );
	zrot[1][0] = -sin( rad );
	zrot[1][1] = cos( rad );

	float tmpmat[3][3];
	MatrixMultiply( m, zrot, tmpmat );
	float rot[3][3];
	MatrixMultiply( tmpmat, im, rot );
	vec3 result;

	for (size_t i = 0; i < 3; i++)
	{
		result[i] = rot[i][0] * x + rot[i][1] * y + rot[i][2] * z;
	}

	return result;
}

vec3 vec3::inverse() const
{
	return vec3(-x, -y, -z);
}

void vec3::invert()
{
	x = -x;
	y = -y;
	z = -z;
}

void vec3::snap()
{
	x = (float)(int)x;
	y = (float)(int)y;
	z = (float)(int)z;
}

/// Round a vector to integers for more efficient network transmission. 
/// 
/// Make sure that it rounds towards a given point rather than blindly truncating. This prevents it from truncating into a wall.
void vec3::snapTowards(const vec3 &v)
{
	for (size_t i = 0 ; i < 3 ; i++)
	{
		if (v[i] <= (&x)[i])
		{
			(&x)[i] = (float)(int)v[i];
		}
		else
		{
			(&x)[i] = (float)(int)v[i] + 1;
		}
	}
}

float vec3::normalize()
{
	const float l = length();

	if (l)
	{
		const float il = 1/l;
		x *= il;
		y *= il;
		z *= il;
	}

	return l;
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
void vec3::normalizeFast()
{
	const float ilength = ReciprocalSqrt(vec3::dotProduct(*this, *this));
	x *= ilength;
	y *= ilength;
	z *= ilength;
}

vec3 vec3::projectOnPlane(const vec3 &normal) const
{
	float inv_denom = vec3::dotProduct(*this, *this);
	assert(fabs(inv_denom) != 0.0f); // Zero vectors fail here.
	inv_denom = 1.0f / inv_denom;

	float d = vec3::dotProduct(*this, normal) * inv_denom;

	return normal - *this * inv_denom * d;
}

} // namespace math
