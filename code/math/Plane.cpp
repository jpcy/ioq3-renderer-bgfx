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

Plane::Plane() : distance_(0)
{
}

Plane::Plane(float a, float b, float c, float d) : normal_(a, b, c), distance_(d)
{
	normalize();
}

Plane::Plane(const vec3 &normal, const float distance) : normal_(normal), distance_(distance)
{
}

vec3 Plane::getNormal() const
{
	return normal_;
}

float Plane::getDistance() const
{
	return distance_;
}

float Plane::calculateDistance(const vec3 &v) const
{
	return vec3::dotProduct(v, normal_) + distance_;
}

Plane::Side Plane::calculateSide(const vec3 &v, const float epsilon) const
{
	const float distance = calculateDistance(v);

	if (distance > epsilon)
	{
		return Front;
	}
	else if (distance < -epsilon)
	{
		return Back;
	}

	return On;
}

Plane Plane::inverse() const
{
	return Plane(normal_.inverse(), -distance_);
}

void Plane::invert()
{
	normal_.invert();
}

void Plane::normalize()
{
	const float length = normal_.length();

	if (length > 0)
	{
		const float ilength = 1.0f / length;
		normal_ *= ilength;
		distance_ *= ilength;
	}
}

} // namespace math
