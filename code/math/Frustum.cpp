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

/**
@file
@brief
*/
#include "Math.h"

namespace math {

Frustum::Frustum(const mat4 &mvp)
{
	// From Xreal R_SetupFrustum2, but with distance not negated.
	planes_[LeftPlane] = Plane(mvp[3] + mvp[0], mvp[7] + mvp[4], mvp[11] + mvp[8], mvp[15] + mvp[12]);
	planes_[RightPlane] = Plane(mvp[3] - mvp[0], mvp[7] - mvp[4], mvp[11] - mvp[8], mvp[15] - mvp[12]);
	planes_[BottomPlane] = Plane(mvp[3] + mvp[1], mvp[7] + mvp[5], mvp[11] + mvp[9], mvp[15] + mvp[13]);
	planes_[TopPlane] = Plane(mvp[3] - mvp[1], mvp[7] - mvp[5], mvp[11] - mvp[9], mvp[15] - mvp[13]);

	planes_[NearPlane] = Plane(mvp[3] + mvp[2], mvp[7] + mvp[6], mvp[11] + mvp[10], mvp[15] + mvp[14]);
	planes_[FarPlane] = Plane(mvp[3] - mvp[2], mvp[7] - mvp[6], mvp[11] - mvp[10], mvp[15] - mvp[14]);
}

Frustum::ClipResult Frustum::clipBounds(const Bounds &bounds, const mat4 &modelMatrix) const
{
	std::array<vec3, 8> corners;

	for (size_t i = 0; i < corners.size(); i++)
	{
		vec3 v;
		v[0] = bounds[i & 1][0];
		v[1] = bounds[(i >> 1) & 1][1];
		v[2] = bounds[(i >> 2) & 1][2];
		corners[i] = modelMatrix.transform(v); // Transform bounds corners to world space.
	}

	return clipBox(corners);
}

Frustum::ClipResult Frustum::clipSphere(const vec3 &position, float radius) const
{
	bool mightBeClipped = false;

	for (size_t i = 0; i < nPlanesToClipAgainst_; i++)
	{
		const float d = planes_[i].calculateDistance(position);

		if (d < -radius)
			return ClipResult::Outside;
		else if (d <= radius)
			mightBeClipped = true;
	}

	if (mightBeClipped)
		return ClipResult::Partial;

	return ClipResult::Inside;
}

bool Frustum::isInside(const vec3 &v) const
{
	for (size_t i = 0; i < nPlanesToClipAgainst_; i++)
	{
		if (planes_[i].calculateSide(v) == Plane::Back)
			return false;
	}

	// Point is in front of all planes.
	return true;
}

Frustum::ClipResult Frustum::clipBox(const std::array<vec3, 8> &corners) const
{
	// Check against frustum planes.
	int anyBack = 0;

	for (size_t i = 0; i < nPlanesToClipAgainst_; i++)
	{
		int front = 0;
		int back = 0;

		for (size_t j = 0; j < corners.size(); j++)
		{
			if (planes_[i].calculateSide(corners[j]) == Plane::Back)
			{
				back = 1;
			}
			else
			{
				front = 1;

				if (back)
					break; // A point is in front.
			}
		}

		if (!front)
			return ClipResult::Outside; // All corners were behind one of the planes.

		anyBack |= back;
	}

	if (!anyBack)
		return ClipResult::Inside; // Completely inside frustum.

	return ClipResult::Partial; // Partially clipped.
}

} // namespace math
