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
#include "Math.h"

namespace math {

Plane::Plane() : distance(0), type_(Type::NonAxial), signBits_(0)
{
}

Plane::Plane(float a, float b, float c, float d) : normal(a, b, c), distance(d), type_(Type::NonAxial), signBits_(0)
{
	normalize();
}

Plane::Plane(vec3 normal, float distance) : normal(normal), distance(distance), type_(Type::NonAxial), signBits_(0)
{
}

float Plane::calculateDistance(const vec3 &v) const
{
	return vec3::dotProduct(v, normal) + distance;
}

Plane::Side Plane::calculateSide(const vec3 &v, const float epsilon) const
{
	const float d = calculateDistance(v);

	if (d > epsilon)
	{
		return Front;
	}
	else if (d < -epsilon)
	{
		return Back;
	}

	return On;
}

Plane Plane::inverse() const
{
	return Plane(normal.inverse(), -distance);
}

void Plane::invert()
{
	normal.invert();
}

void Plane::setupFastBoundsTest()
{
	if (normal.x == 1.0f)
		type_ = Type::AxialX;
	else if (normal.y == 1.0f)
		type_ = Type::AxialY;
	else if (normal.z == 1.0f)
		type_ = Type::AxialZ;
	else
		type_ = Type::NonAxial;

	signBits_ = 0;

	for (int i = 0; i < 3; i++)
	{
		if (normal[i] < 0)
		{
			signBits_ |= 1 << i;
		}
	}
}

int Plane::testBounds(Bounds bounds)
{
	// Fast axial cases.
	if (type_ != Type::NonAxial)
	{
		if (distance <= bounds.min[(uint8_t)type_])
			return 1;
		if (distance >= bounds.max[(uint8_t)type_])
			return 2;

		return 3;
	}

	// General case.
	float dist[2];
	dist[0] = dist[1] = 0;

	if (signBits_ < 8) // >= 8: default case is original code (dist[0]=dist[1]=0)
	{
		for (int i = 0; i < 3; i++)
		{
			const int b = (signBits_ >> i) & 1;
			dist[b] += normal[i] * bounds.max[i];
			dist[!b] += normal[i] * bounds.min[i];
		}
	}

	int sides = 0;

	if (dist[0] >= distance)
		sides = 1;
	if (dist[1] < distance)
		sides |= 2;

	return sides;
}

void Plane::normalize()
{
	const float length = normal.length();

	if (length > 0)
	{
		const float ilength = 1.0f / length;
		normal *= ilength;
		distance *= ilength;
	}
}

} // namespace math
