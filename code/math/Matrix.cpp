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
/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// OpenGL Mathematics (glm.g-truc.net)
//
// Copyright (c) 2005 - 2013 G-Truc Creation (www.g-truc.net)
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// ref gtc_matrix_transform
// file glm/gtc/matrix_transform.inl
// date 2009-04-29 / 2011-06-15
// author Christophe Riccio

// Sources: XreaL, "The Matrix and Quaternions FAQ".

#include "Math.h"

namespace math {

const mat3 mat3::identity;

mat3::mat3()
{
	rows_[0] = vec3(1, 0, 0);
	rows_[1] = vec3(0, 1, 0);
	rows_[2] = vec3(0, 0, 1);
}

mat3::mat3(const vec3 &a1, const vec3 &a2, const vec3 &a3)
{
	rows_[0] = a1;
	rows_[1] = a2;
	rows_[2] = a3;
}

mat3::mat3(const vec3 &angles)
{
	vec3 right;

	// Angle vectors returns "right" instead of "y axis".
	angles.toAngleVectors(&rows_[0], &right, &rows_[2]);
	rows_[1] = vec3::empty - right;
}

mat3::mat3(const mat4 &m)
{
	rows_[0] = vec3(m[0], m[1], m[2]);
	rows_[1] = vec3(m[4], m[5], m[6]);
	rows_[2] = vec3(m[8], m[9], m[10]);
}

mat3::mat3(const float axis[3][3])
{
	rows_[0] = axis[0];
	rows_[1] = axis[1];
	rows_[2] = axis[2];
}

void mat3::transpose()
{
	mat3 o(*this);
	rows_[0] = vec3(o[0][0], o[1][0], o[2][0]);
	rows_[1] = vec3(o[0][1], o[1][1], o[2][1]);
	rows_[2] = vec3(o[0][2], o[1][2], o[2][2]);
}

void mat3::rotateAroundDirection(float yaw)
{
	// Create an arbitrary axis 1.
	rows_[1] = rows_[0].perpendicular();

	// Rotate it around axis 0 by yaw.
	if (yaw)
	{
		rows_[1] = rows_[1].rotated(rows_[0], yaw);
	}

	// Cross to get axis 2.
	rows_[2] = vec3::crossProduct(rows_[0], rows_[1]);
}

float mat3::determinate() const
{
	return rows_[0][0] * (rows_[1][1]*rows_[2][2] - rows_[1][2] * rows_[2][1]) - rows_[0][1] * (rows_[1][0]*rows_[2][2] - rows_[1][2] * rows_[2][0]) + rows_[0][2] * (rows_[1][0]*rows_[2][1] - rows_[1][1] * rows_[2][0]);
}

mat3 mat3::inverse() const
{
	const float s = 1.0f / determinate();

	mat3 m;
	m[0][0] = s * (rows_[1][1] * rows_[2][2] - rows_[1][2] * rows_[2][1]);
	m[1][0] = s * (rows_[1][2] * rows_[2][0] - rows_[1][0] * rows_[2][2]);
	m[2][0] = s * (rows_[1][0] * rows_[2][1] - rows_[1][1] * rows_[2][0]);
	m[0][1] = s * (rows_[0][2] * rows_[2][1] - rows_[0][1] * rows_[2][2]);
	m[1][1] = s * (rows_[0][0] * rows_[2][2] - rows_[0][2] * rows_[2][0]);
	m[2][1] = s * (rows_[0][1] * rows_[2][0] - rows_[0][0] * rows_[2][1]);
	m[0][2] = s * (rows_[0][1] * rows_[1][2] - rows_[0][2] * rows_[1][1]);
	m[1][2] = s * (rows_[0][2] * rows_[1][0] - rows_[0][0] * rows_[1][2]);
	m[2][2] = s * (rows_[0][0] * rows_[1][1] - rows_[0][1] * rows_[1][0]);
	return m;
}

vec3 mat3::transform(const vec3 &v) const
{
	return vec3
	(
		rows_[0][0] * v.x + rows_[0][1] * v.y + rows_[0][2] * v.z,
		rows_[1][0] * v.x + rows_[1][1] * v.y + rows_[1][2] * v.z,
		rows_[2][0] * v.x + rows_[2][1] * v.y + rows_[2][2] * v.z
	);
}

vec3 &mat3::operator[](size_t index)
{
	assert(index >= 0 && index <= 2);
	return rows_[index];
}

const vec3 &mat3::operator[](size_t index) const
{
	assert(index >= 0 && index <= 2);
	return rows_[index];
}

mat3 mat3::operator*(const mat3 &m) const
{
	mat3 out;

	out[0][0] = m[0][0] * rows_[0][0] + m[0][1] * rows_[1][0] + m[0][2] * rows_[2][0];
	out[0][1] = m[0][0] * rows_[0][1] + m[0][1] * rows_[1][1] + m[0][2] * rows_[2][1];
	out[0][2] = m[0][0] * rows_[0][2] + m[0][1] * rows_[1][2] + m[0][2] * rows_[2][2];
	out[1][0] = m[1][0] * rows_[0][0] + m[1][1] * rows_[1][0] + m[1][2] * rows_[2][0];
	out[1][1] = m[1][0] * rows_[0][1] + m[1][1] * rows_[1][1] + m[1][2] * rows_[2][1];
	out[1][2] = m[1][0] * rows_[0][2] + m[1][1] * rows_[1][2] + m[1][2] * rows_[2][2];
	out[2][0] = m[2][0] * rows_[0][0] + m[2][1] * rows_[1][0] + m[2][2] * rows_[2][0];
	out[2][1] = m[2][0] * rows_[0][1] + m[2][1] * rows_[1][1] + m[2][2] * rows_[2][1];
	out[2][2] = m[2][0] * rows_[0][2] + m[2][1] * rows_[1][2] + m[2][2] * rows_[2][2];

	return out;
}

// From glm matrix_transform.inl rotate.
mat3 mat3::rotation(float degrees, const vec3 &axis)
{
	const float r = DEG2RAD(degrees);
	const float c = cos(r);
	const float s = sin(r);
	const vec3 na = axis.normal();
	const vec3 temp = na * (1.0f - c);

	mat3 m;
	m[0] = vec3(c + temp[0] * na[0], temp[0] * na[1] + s * na[2], temp[0] * na[2] - s * na[1]);
	m[1] = vec3(temp[1] * na[0] - s * na[2], c + temp[1] * na[1], temp[1] * na[2] + s * na[0]);
	m[2] = vec3(temp[2] * na[0] + s * na[1], temp[2] * na[1] - s * na[0], c + temp[2] * na[2]);
	return m;
}

mat3 mat3::rotationX(float degrees)
{
	const float r = DEG2RAD(degrees);
	mat3 m;
	m[0] = vec3(1, 0, 0);
	m[1] = vec3(0, cos(r), sin(r));
	m[2] = vec3(0, -sin(r), cos(r));
	return m;
}

mat3 mat3::rotationY(float degrees)
{
	const float r = DEG2RAD(degrees);
	mat3 m;
	m[0] = vec3(cos(r), 0, -sin(r));
	m[1] = vec3(0, 1, 0);
	m[2] = vec3(sin(r), 0, cos(r));
	return m;
}

mat3 mat3::rotationZ(float degrees)
{
	const float r = DEG2RAD(degrees);
	mat3 m;
	m[0] = vec3(cos(r), sin(r), 0);
	m[1] = vec3(-sin(r), cos(r), 0);
	m[2] = vec3(0, 0, 1);
	return m;
}

const float identityElements[] = 
{
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

const mat4 mat4::empty;
const mat4 mat4::identity(identityElements);

mat4::mat4() : e_()
{
}

mat4::mat4(const float *m)
{
	e_[0] = m[0];    e_[4] = m[4];    e_[ 8] = m[ 8];    e_[12] = m[12];
	e_[1] = m[1];    e_[5] = m[5];    e_[ 9] = m[ 9];    e_[13] = m[13];
	e_[2] = m[2];    e_[6] = m[6];    e_[10] = m[10];    e_[14] = m[14];
	e_[3] = m[3];    e_[7] = m[7];    e_[11] = m[11];    e_[15] = m[15];
}

mat4::mat4(float e0, float e1, float e2, float e3, float e4, float e5, float e6, float e7, float e8, float e9, float e10, float e11, float e12, float e13, float e14, float e15)
{
	e_[0] = e0;    e_[4] = e4;    e_[ 8] = e8;    e_[12] = e12;
	e_[1] = e1;    e_[5] = e5;    e_[ 9] = e9;    e_[13] = e13;
	e_[2] = e2;    e_[6] = e6;    e_[10] = e10;    e_[14] = e14;
	e_[3] = e3;    e_[7] = e7;    e_[11] = e11;    e_[15] = e15;
}

mat4::mat4(const mat3 &m)
{
	e_[0] = m[0][0]; e_[4] = m[1][0]; e_[ 8] = m[2][0]; e_[12] = 0;
	e_[1] = m[0][1]; e_[5] = m[1][1]; e_[ 9] = m[2][1]; e_[13] = 0;
	e_[2] = m[0][2]; e_[6] = m[1][2]; e_[10] = m[2][2]; e_[14] = 0;
	e_[3] = 0;       e_[7] = 0;       e_[11] = 0;       e_[15] = 1;
}

bool mat4::equals(const mat4 &m) const
{
	return (e_[0] == m[0] && e_[4] == m[4] && e_[ 8] == m[ 8] && e_[12] == m[12] &&
			e_[1] == m[1] && e_[5] == m[5] && e_[ 9] == m[ 9] && e_[13] == m[13] &&
			e_[2] == m[2] && e_[6] == m[6] && e_[10] == m[10] && e_[14] == m[14] &&
			e_[3] == m[3] && e_[7] == m[7] && e_[11] == m[11] && e_[15] == m[15]);
}

Bounds mat4::transform(const Bounds &bounds) const
{
	std::array<vec3, 8> v = bounds.toVertices();

	for (int i = 0; i < 8; i++)
	{
		v[i] = transform(v[i]);
	}

	Bounds b;
	b.setupForAddingPoints();
	b.addPoints(v.data(), v.size());
	return b;
}

vec3 mat4::transform(const vec3 &v) const
{
	return vec3
	(
		e_[0] * v[0] + e_[4] * v[1] + e_[ 8] * v[2] + e_[12],
		e_[1] * v[0] + e_[5] * v[1] + e_[ 9] * v[2] + e_[13],
		e_[2] * v[0] + e_[6] * v[1] + e_[10] * v[2] + e_[14]
	);
}

vec4 mat4::transform(const vec4 &v) const
{
	return vec4
	(
		e_[0] * v[0] + e_[4] * v[1] + e_[ 8] * v[2] + e_[12] * v[3],
		e_[1] * v[0] + e_[5] * v[1] + e_[ 9] * v[2] + e_[13] * v[3],
		e_[2] * v[0] + e_[6] * v[1] + e_[10] * v[2] + e_[14] * v[3],
		e_[3] * v[0] + e_[7] * v[1] + e_[11] * v[2] + e_[15] * v[3]
	);
}

vec3 mat4::transformNormal(const vec3 &n) const
{
	return vec3(e_[0] * n[0] + e_[4] * n[1] + e_[ 8] * n[2],
					e_[1] * n[0] + e_[5] * n[1] + e_[ 9] * n[2],
					e_[2] * n[0] + e_[6] * n[1] + e_[10] * n[2]);
}

float mat4::determinate() const
{
	float det, result = 0, i = 1;
	float sub[9];
	int n;

	for (n = 0; n < 4; n++, i *= -1)
	{
		calculateSubmat3x3(sub, 0, n);
		det = calculateDeterminate3x3(sub);
		result += e_[n] * det * i;
	}

	return result;
}

void mat4::extract(mat3 *rotation, vec3 *translation) const
{
	if (rotation)
	{
		(*rotation)[0] = vec3(e_[0], e_[1], e_[2]);
		(*rotation)[1] = vec3(e_[4], e_[5], e_[6]);
		(*rotation)[2] = vec3(e_[8], e_[9], e_[10]);
	}

	if (translation)
	{
		*translation = vec3(e_[12], e_[13], e_[14]);
	}
}

void mat4::copy(const mat4 &m)
{
	e_[0] = m[0];    e_[4] = m[4];    e_[ 8] = m[ 8];    e_[12] = m[12];
	e_[1] = m[1];    e_[5] = m[5];    e_[ 9] = m[ 9];    e_[13] = m[13];
	e_[2] = m[2];    e_[6] = m[6];    e_[10] = m[10];    e_[14] = m[14];
	e_[3] = m[3];    e_[7] = m[7];    e_[11] = m[11];    e_[15] = m[15];
}

void mat4::transpose()
{
	mat4 o(*this);

	e_[ 0] = o[0];    e_[ 1] = o[4];    e_[ 2] = o[ 8];    e_[ 3] = o[12];
	e_[ 4] = o[1];    e_[ 5] = o[5];    e_[ 6] = o[ 9];    e_[ 7] = o[13];
	e_[ 8] = o[2];    e_[ 9] = o[6];    e_[10] = o[10];    e_[11] = o[14];
	e_[12] = o[3];    e_[13] = o[7];    e_[14] = o[11];    e_[15] = o[15];
}

void mat4::setupScale(float scale)
{
	e_[0] = scale; e_[4] = 0;     e_[ 8] = 0;     e_[12] = 0;
	e_[1] = 0;     e_[5] = scale; e_[ 9] = 0;     e_[13] = 0;
	e_[2] = 0;     e_[6] = 0;     e_[10] = scale; e_[14] = 0;
	e_[3] = 0;     e_[7] = 0;     e_[11] = 0;     e_[15] = 1;
}

void mat4::setupScale(float sx, float sy, float sz)
{
	e_[0] = sx;   e_[4] = 0;    e_[ 8] = 0;    e_[12] = 0;
	e_[1] = 0;    e_[5] = sy;   e_[ 9] = 0;    e_[13] = 0;
	e_[2] = 0;    e_[6] = 0;    e_[10] = sz;   e_[14] = 0;
	e_[3] = 0;    e_[7] = 0;    e_[11] = 0;    e_[15] = 1;
}

void mat4::setupScale(const vec3 &v)
{
	e_[0] = v[0]; e_[4] = 0;    e_[ 8] = 0;    e_[12] = 0;
	e_[1] = 0;    e_[5] = v[1]; e_[ 9] = 0;    e_[13] = 0;
	e_[2] = 0;    e_[6] = 0;    e_[10] = v[2]; e_[14] = 0;
	e_[3] = 0;    e_[7] = 0;    e_[11] = 0;    e_[15] = 1;
}

void mat4::setupTransform(const mat4 &rot, const vec3 &origin)
{
	e_[0] = rot[0];    e_[4] = rot[4];    e_[ 8] = rot[ 8];    e_[12] = origin[0];
	e_[1] = rot[1];    e_[5] = rot[5];    e_[ 9] = rot[ 9];    e_[13] = origin[1];
	e_[2] = rot[2];    e_[6] = rot[6];    e_[10] = rot[10];    e_[14] = origin[2];
	e_[3] = 0;         e_[7] = 0;         e_[11] = 0;          e_[15] = 1;
}

void mat4::setupTransform(const mat3 &rot, const vec3 &origin)
{
	e_[0] = rot[0][0]; e_[4] = rot[1][0]; e_[ 8] = rot[2][0]; e_[12] = origin[0];
	e_[1] = rot[0][1]; e_[5] = rot[1][1]; e_[ 9] = rot[2][1]; e_[13] = origin[1];
	e_[2] = rot[0][2]; e_[6] = rot[1][2]; e_[10] = rot[2][2]; e_[14] = origin[2];
	e_[3] = 0;         e_[7] = 0;         e_[11] = 0;         e_[15] = 1;
}

void mat4::setupOrthographicProjection(int left, int right, int top, int bottom)
{
	// http://en.wikipedia.org/wiki/Orthographic_projection_%28geometry%29
	// near: 0, far: 1
	const float l = static_cast<float>(left);
	const float r = static_cast<float>(right);
	const float t = static_cast<float>(top);
	const float b = static_cast<float>(bottom);

	e_[0] = 2 / (r-l); e_[4] = 0;         e_[ 8] = 0;  e_[12] = -(r+l)/(r-l);
	e_[1] = 0;         e_[5] = 2 / (t-b); e_[ 9] = 0;  e_[13] = -(t+b)/(t-b);
	e_[2] = 0;         e_[6] = 0;         e_[10] = -2; e_[14] = -1;
	e_[3] = 0;         e_[7] = 0;         e_[11] = 0;  e_[15] = 1;
}

void mat4::setupOrthographicProjection(const Bounds &bounds)
{
	/*
	x = -y
	y = x

	Looking straight down (-Z), in our coordinate system:

		X
		|
	Y --*

	Converting to:
			
	Y
	|
	* --X
	*/
	const float l = -bounds[1][1];
	const float r = -bounds[0][1];
	const float t = bounds[0][0];
	const float b = bounds[1][0];
	const float zn = bounds[0][2];
	const float zf = bounds[1][2];
	setupOrthographicProjection(l, r, t, b, zn, zf);
}

/*
same as D3DXMatrixOrthoOffCenterRH

http://msdn.microsoft.com/en-us/library/bb205348(VS.85).aspx
*/
void mat4::setupOrthographicProjection(float l, float r, float t, float b, float zn, float zf)
{
	e_[0] = 2/(r-l); e_[4] = 0;       e_[8] = 0;          e_[12] = (l + r) / (l - r);
	e_[1] = 0;       e_[5] = 2/(t-b); e_[9] = 0;          e_[13] = (t + b) / (b - t);
	e_[2] = 0;       e_[6] = 0;       e_[10] = 1/(zn-zf); e_[14] = zn / (zn - zf);
	e_[3] = 0;       e_[7] = 0;       e_[11] = 0;         e_[15] = 1;
}

void mat4::setupPerspectiveProjection(float fovX, float fovY, float zn, float zf)
{
	// http://www.songho.ca/opengl/gl_projectionmatrix.html
	// Symmetrical left/right and top/bottom.
	const float r = tan(DEG2RAD(fovX) / 2.0f) * zn;
	const float t = tan(DEG2RAD(fovY) / 2.0f) * zn;

	e_[0] = zn/r; e_[4] = 0;    e_[ 8] = 0;                e_[12] = 0;
	e_[1] = 0;    e_[5] = zn/t; e_[ 9] = 0;                e_[13] = 0;
	e_[2] = 0;    e_[6] = 0;    e_[10] = -(zf+zn)/(zf-zn); e_[14] = (-2*zf*zn)/(zf-zn);
	e_[3] = 0;    e_[7] = 0;    e_[11] = -1;               e_[15] = 0;
}

void mat4::invert()
{
	int i, j, sign;
	float sub[9];
	float det = determinate();
	mat4 copy(*this);

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			sign = 1 - ((i+j) % 2) * 2;
			copy.calculateSubmat3x3(sub, i, j);
			e_[i+j*4] = (calculateDeterminate3x3(sub) * sign ) / det;
		}
	}
}

const float &mat4::operator[](size_t i) const
{
	return e_[i];
}

float &mat4::operator[](size_t i)
{
	return e_[i];
}

mat4 mat4::operator*(const mat4 &m) const
{
	mat4 out;

	out[ 0] = m[ 0]*e_[ 0] + m[ 1]*e_[ 4] + m[ 2]*e_[ 8] + m[ 3]*e_[12];
	out[ 1] = m[ 0]*e_[ 1] + m[ 1]*e_[ 5] + m[ 2]*e_[ 9] + m[ 3]*e_[13];
	out[ 2] = m[ 0]*e_[ 2] + m[ 1]*e_[ 6] + m[ 2]*e_[10] + m[ 3]*e_[14];
	out[ 3] = m[ 0]*e_[ 3] + m[ 1]*e_[ 7] + m[ 2]*e_[11] + m[ 3]*e_[15];

	out[ 4] = m[ 4]*e_[ 0] + m[ 5]*e_[ 4] + m[ 6]*e_[ 8] + m[ 7]*e_[12];
	out[ 5] = m[ 4]*e_[ 1] + m[ 5]*e_[ 5] + m[ 6]*e_[ 9] + m[ 7]*e_[13];
	out[ 6] = m[ 4]*e_[ 2] + m[ 5]*e_[ 6] + m[ 6]*e_[10] + m[ 7]*e_[14];
	out[ 7] = m[ 4]*e_[ 3] + m[ 5]*e_[ 7] + m[ 6]*e_[11] + m[ 7]*e_[15];

	out[ 8] = m[ 8]*e_[ 0] + m[ 9]*e_[ 4] + m[10]*e_[ 8] + m[11]*e_[12];
	out[ 9] = m[ 8]*e_[ 1] + m[ 9]*e_[ 5] + m[10]*e_[ 9] + m[11]*e_[13];
	out[10] = m[ 8]*e_[ 2] + m[ 9]*e_[ 6] + m[10]*e_[10] + m[11]*e_[14];
	out[11] = m[ 8]*e_[ 3] + m[ 9]*e_[ 7] + m[10]*e_[11] + m[11]*e_[15];

	out[12] = m[12]*e_[ 0] + m[13]*e_[ 4] + m[14]*e_[ 8] + m[15]*e_[12];
	out[13] = m[12]*e_[ 1] + m[13]*e_[ 5] + m[14]*e_[ 9] + m[15]*e_[13];
	out[14] = m[12]*e_[ 2] + m[13]*e_[ 6] + m[14]*e_[10] + m[15]*e_[14];
	out[15] = m[12]*e_[ 3] + m[13]*e_[ 7] + m[14]*e_[11] + m[15]*e_[15];

	return out;
}

void mat4::operator*=(const mat4 &m)
{
	copy(*this * m);
}

mat4 mat4::perspectiveProjection(float fovX, float fovY, float zNear, float zFar)
{
	mat4 m;
	m.setupPerspectiveProjection(fovX, fovY, zNear, zFar);
	return m;
}

mat4 mat4::perspectiveProjection(float l, float r, float t, float b, float n, float f)
{
	mat4 m;
	/***********************************************************
	* A single header file OpenGL lightmapping library         *
	* https://github.com/ands/lightmapper                      *
	* no warranty implied | use at your own risk               *
	* author: Andreas Mantler (ands) | last change: 23.07.2016 *
	*                                                          *
	* License:                                                 *
	* This software is in the public domain.                   *
	* Where that dedication is not recognized,                 *
	* you are granted a perpetual, irrevocable license to copy *
	* and modify this file however you want.                   *
	***********************************************************/
	// projection matrix: frustum(l, r, b, t, n, f)
	float ilr = 1.0f / (r - l), ibt = 1.0f / (t - b), ninf = -1.0f / (f - n), n2 = 2.0f * n;
	m.e_[ 0] = n2 * ilr;      m.e_[ 1] = 0.0f;          m.e_[ 2] = 0.0f;           m.e_[ 3] = 0.0f;
	m.e_[ 4] = 0.0f;          m.e_[ 5] = n2 * ibt;      m.e_[ 6] = 0.0f;           m.e_[ 7] = 0.0f;
	m.e_[ 8] = (r + l) * ilr; m.e_[ 9] = (t + b) * ibt; m.e_[10] = (f + n) * ninf; m.e_[11] = -1.0f;
	m.e_[12] = 0.0f;          m.e_[13] = 0.0f;          m.e_[14] = f * n2 * ninf;  m.e_[15] = 0.0f;
	return m;
}

mat4 mat4::orthographicProjection(float l, float r, float t, float b, float zn, float zf)
{
	mat4 m;
	m.setupOrthographicProjection(l, r, t, b, zn, zf);
	return m;
}

mat4 mat4::orthographicProjection(const Bounds &bounds)
{
	mat4 m;
	m.setupOrthographicProjection(bounds);
	return m;
}

mat4 mat4::view(const vec3 &position, const mat3 &rotation)
{
	mat4 m;
	m[0] = rotation[0][0];
	m[1] = rotation[1][0];
	m[2] = rotation[2][0];
	m[3] = 0;

	m[4] = rotation[0][1];
	m[5] = rotation[1][1];
	m[6] = rotation[2][1];
	m[7] = 0;

	m[8] = rotation[0][2];
	m[9] = rotation[1][2];
	m[10] = rotation[2][2];
	m[11] = 0;

	m[12] = -position[0] * m[0] + -position[1] * m[4] + -position[2] * m[8];
	m[13] = -position[0] * m[1] + -position[1] * m[5] + -position[2] * m[9];
	m[14] = -position[0] * m[2] + -position[1] * m[6] + -position[2] * m[10];
	m[15] = 1;
	return m;
}

mat4 mat4::lookAt(const vec3 &eye, const vec3 &direction, const vec3 &up)
{
	const vec3 sideN = vec3::crossProduct(direction, up).normal();
	const vec3 upN = vec3::crossProduct(sideN, direction).normal();
	const vec3 dirN = direction.normal();

	mat4 m;
	m[ 0] = sideN[0];	m[ 4] = sideN[1];		m[ 8] = sideN[2];		m[12] = -vec3::dotProduct(sideN, eye);
	m[ 1] = upN[0];		m[ 5] = upN[1];			m[ 9] = upN[2];			m[13] = -vec3::dotProduct(upN, eye);
	m[ 2] = -dirN[0];	m[ 6] = -dirN[1];		m[10] = -dirN[2];		m[14] = vec3::dotProduct(dirN, eye);
	m[ 3] = 0;			m[ 7] = 0;				m[11] = 0;				m[15] = 1;
	return m;
}

mat4 mat4::translate(const vec3 &position)
{
	mat4 m;
	m.setupTransform(mat3::identity, position);
	return m;
}

mat4 mat4::scale(const vec3 &v)
{
	mat4 m;
	m.setupScale(v);
	return m;
}

mat4 mat4::transform(const mat3 &rot, const vec3 &origin)
{
	mat4 m;
	m.setupTransform(rot, origin);
	return m;
}

mat4 mat4::crop(const Bounds &bounds)
{
	const float scaleX = 2.0f / (bounds[1][0] - bounds[0][0]);
	const float scaleY = 2.0f / (bounds[1][1] - bounds[0][1]);
	const float offsetX = -0.5f * (bounds[1][0] + bounds[0][0]) * scaleX;
	const float offsetY = -0.5f * (bounds[1][1] + bounds[0][1]) * scaleY;
	const float scaleZ = 1.0f / (bounds[1][2] - bounds[0][2]);
	const float offsetZ = -bounds[0][2] * scaleZ;

	mat4 m;
	m[ 0] = scaleX;		m[ 4] = 0;			m[ 8] = 0;      	m[12] = offsetX;
	m[ 1] = 0;			m[ 5] = scaleY;		m[ 9] = 0;      	m[13] = offsetY;
	m[ 2] = 0;			m[ 6] = 0;      	m[10] = scaleZ;		m[14] = offsetZ;
	m[ 3] = 0;			m[ 7] = 0;			m[11] = 0;			m[15] = 1;
	return m;
}

void mat4::calculateSubmat3x3(float *e3x3, int i, int j) const
{
	assert(e3x3 != 0);

	int ti, tj, idst = 0, jdst = 0;

	for (ti = 0; ti < 4; ti++)
	{
		if (ti < i)
		{
			idst = ti;
		}
		else
		{
			if (ti > i)
			{
				idst = ti-1;
			}
		}

		for (tj = 0; tj < 4; tj++)
		{
			if (tj < j)
			{
				jdst = tj;
			}
			else
			{
				if (tj > j)
				{
					jdst = tj-1;
				}
			}

			if (ti != i && tj != j)
				e3x3[idst*3 + jdst] = e_[ti*4 + tj];
		}
	}
}

float mat4::calculateDeterminate3x3(float *e3x3) const
{
	return e3x3[0] * (e3x3[4]*e3x3[8] - e3x3[7]*e3x3[5]) - e3x3[1] * (e3x3[3]*e3x3[8] - e3x3[6]*e3x3[5]) + e3x3[2] * (e3x3[3]*e3x3[7] - e3x3[6]*e3x3[4]);
}

} // namespace math
