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

const Bounds Bounds::empty(vec3::empty, vec3::empty);

Bounds::Bounds()
{
}

Bounds::Bounds(const vec3 &min, const vec3 &max)
{
	this->min = min;
	this->max = max;
}

Bounds::Bounds(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
{
	min = vec3(minX, minY, minZ);
	max = vec3(maxX, maxY, maxZ);
}

Bounds::Bounds(const vec3 &origin, float radius)
{
	min = origin - vec3(radius);
	max = origin + vec3(radius);
}

float Bounds::toRadius() const
{
	float a, b;
	vec3 corner;

	a = fabs(min[0]);
	b = fabs(max[0]);
	corner[0] = a > b ? a : b;

	a = fabs(min[1]);
	b = fabs(max[1]);
	corner[1] = a > b ? a : b;

	a = fabs(min[2]);
	b = fabs(max[2]);
	corner[2] = a > b ? a : b;

	return corner.length();
}

vec3 Bounds::toSize() const
{
	return max - min;
}

void Bounds::toVertices(vec3 *vertices) const
{
	assert(vertices);

	// Top, starting at min X and Y, counter-clockwise winding.
	vertices[0] = vec3(min[0], min[1], max[2]);
	vertices[1] = vec3(max[0], min[1], max[2]);
	vertices[2] = vec3(max[0], max[1], max[2]);
	vertices[3] = vec3(min[0], max[1], max[2]);

	// Bottom, starting at min X and Y, counter-clockwise winding.
	vertices[4] = vec3(min[0], min[1], min[2]);
	vertices[5] = vec3(min[0], max[1], min[2]);
	vertices[6] = vec3(max[0], max[1], min[2]);
	vertices[7] = vec3(max[0], min[1], min[2]);
}

std::array<vec3, 8> Bounds::toVertices() const
{
	std::array<vec3, 8> v;
	toVertices(v.data());
	return v;
}

Bounds Bounds::toModelSpace() const
{
	const vec3 halfExtents = (max - min) / 2.0f;
	return Bounds(halfExtents * -1, halfExtents);
}

bool Bounds::intersectSphere(const vec3 &v, float radius) const
{
	if (v[0] - radius > max[0] ||
		v[0] + radius < min[0] ||
		v[1] - radius > max[1] ||
		v[1] + radius < min[1] ||
		v[2] - radius > max[2] ||
		v[2] + radius < min[2])
	{
		return false;
	}

	return true;
}

bool Bounds::intersectPoint(const vec3 &v) const
{
	if (v[0] > max[0] ||
		v[0] < min[0] ||
		v[1] > max[1] ||
		v[1] < min[1] ||
		v[2] > max[2] ||
		v[2] < min[2])
	{
		return false;
	}

	return true;
}

bool Bounds::intersectPoint(const vec3 &v, float epsilon) const
{
	if (max[0] < v[0] - epsilon ||
		max[1] < v[1] - epsilon ||
		max[2] < v[2] - epsilon ||
		min[0] > v[0] + epsilon ||
		min[1] > v[1] + epsilon ||
		min[2] > v[2] + epsilon)
	{
		return false;
	}

	return true;
}

float Bounds::calculateFarthestCornerDistance(const vec3 &pos) const
{
	float distanceSquared = 0;

	for (size_t i = 0; i < 8; i++)
	{
		const float ds = vec3::distanceSquared(pos, vec3
		(
			(i & 1) ? min[0] : max[0],
			(i & 2) ? min[1] : max[1],
			(i & 4) ? min[2] : max[2]
		));

		if (ds > distanceSquared)
		{
			distanceSquared = ds;
		}
	}

	return sqrt(distanceSquared);
}

void Bounds::expand(float amount)
{
	vec3 v(amount);
	min -= v;
	max += v;
}

vec3 Bounds::midpoint() const
{
	return min + (max - min) * 0.5f;
}

void Bounds::setupForAddingPoints()
{
	min = vec3(99999, 99999, 99999);
	max = vec3(-99999, -99999, -99999);
}

void Bounds::addPoint(const vec3 &v)
{
	if (v[0] < min[0])
	{
		min[0] = v[0];
	}

	if (v[0] > max[0])
	{
		max[0] = v[0];
	}

	if (v[1] < min[1])
	{
		min[1] = v[1];
	}

	if (v[1] > max[1])
	{
		max[1] = v[1];
	}

	if (v[2] < min[2])
	{
		min[2] = v[2];
	}

	if (v[2] > max[2])
	{
		max[2] = v[2];
	}
}

void Bounds::addPoints(const vec3 *points, size_t nPoints)
{
	assert(points);

	for (size_t i = 0; i < nPoints; i++)
	{
		addPoint(points[i]);
	}
}

void Bounds::addPoints(const Bounds &b)
{
	for (int i = 0; i < 8; i++)
	{
		vec3 v;

		if (i & 1)
		{
			v[0] = b[1][0];
		}
		else
		{
			v[0] = b[0][0];
		}

		if ((i >> 1) & 1)
		{
			v[1] = b[1][1];
		}
		else
		{
			v[1] = b[0][1];
		}

		if ((i >> 2) & 1)
		{
			v[2] = b[1][2];
		}
		else
		{
			v[2] = b[0][2];
		}

		addPoint(v);
	}
}

const vec3 &Bounds::operator[](size_t i) const
{
	assert(i == 0 || i == 1);
	return i == 0 ? min : max;
}

vec3 &Bounds::operator[](size_t i)
{
	assert(i == 0 || i == 1);
	return i == 0 ? min : max;
}

Bounds Bounds::operator+(const vec3 &v) const
{
	return Bounds(min + v, max + v);
}

bool Bounds::operator==(const Bounds &b) const
{
	return min == b.min && max == b.max;
}

bool Bounds::intersect(const Bounds &b1, const Bounds &b2)
{
	if (b1.max[0] < b2.min[0] ||
		b1.max[1] < b2.min[1] ||
		b1.max[2] < b2.min[2] ||
		b1.min[0] > b2.max[0] ||
		b1.min[1] > b2.max[1] ||
		b1.min[2] > b2.max[2])
	{
		return false;
	}

	return true;
}

bool Bounds::intersect(const Bounds &b1, const Bounds &b2, float epsilon)
{
	if (b1.max[0] < b2.min[0] - epsilon ||
		b1.max[1] < b2.min[1] - epsilon ||
		b1.max[2] < b2.min[2] - epsilon ||
		b1.min[0] > b2.max[0] + epsilon ||
		b1.min[1] > b2.max[1] + epsilon ||
		b1.min[2] > b2.max[2] + epsilon)
	{
		return false;
	}

	return true;
}

Bounds Bounds::merge(const Bounds &b1, const Bounds &b2)
{
	Bounds result;
	result.setupForAddingPoints();
	result.addPoints(b1);
	result.addPoints(b2);
	return result;
}

} // namespace math
