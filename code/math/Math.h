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
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <array>

#undef min
#undef max

#ifndef M_PI
#define M_PI		3.14159265358979323846f	// matches value in gcc v2 math.h
#endif

#define DEG2RAD( a ) ( ( (a) * (float)M_PI ) / 180.0F )
#define RAD2DEG( a ) ( ( (a) * 180.0f ) / (float)M_PI )

// angle indexes
#define	PITCH 0 // up / down
#define	YAW   1 // left / right
#define	ROLL  2 // fall over

namespace math {

class mat4;

/// Always returns a value from -180 to 180.
inline float AngleSubtract(float a1, float a2)
{
	float a = a1 - a2;

	while (a > 180)
		a -= 360;

	while (a < -180)
		a += 360;

	return a;
}

/*
the msvc acos doesn't always return a value between -PI and PI:

int i;
i = 1065353246;
acos(*(float*) &i) == -1.#IND0
*/
inline float ArcCos(float c)
{
	float angle = acos(c);

	if (angle > M_PI)
		return (float)M_PI;

	if (angle < -M_PI)
		return (float)M_PI;

	return angle;
}

template<class T>
inline T Clamped(T value, const T &lowerClamp, const T &upperClamp)
{
	return std::max(lowerClamp, std::min(upperClamp, value));
}

inline bool FuzzyEquals(float a, float b, float epsilon = 0.001f)
{
	return fabs(a - b) < epsilon;
}

template<typename T>
inline bool IsPowerOfTwo(T x)
{
	return x != 0 && !(x & (x - 1));
}

template<typename T>
inline T Lerp(T from, T to, float fraction)
{
	return T(from + (to - from) * fraction);
}

inline float RandomFloat(float min, float max)
{
	return min + rand() / (RAND_MAX / (max - min));
}

typedef union {
	float f;
	int i;
	unsigned int ui;
} floatint_t;

inline float ReciprocalSqrt(float number)
{
	floatint_t t;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	t.f = number;
	t.i = 0x5f3759df - (t.i >> 1);               // what the fuck?
	y = t.f;
	y = y * (threehalfs - (x2 * y * y));   // 1st iteration
										   //	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}

inline int Sign(float value, float epsilon = 0.001f)
{
	if (value < -epsilon)
		return -1;
	else if (value > epsilon)
		return 1;

	return 0;
}

class vec2
{
public:
	vec2() : x(0), y(0) {}
	vec2(float x, float y) : x(x), y(y) {}
	
	vec2(const float *v)
	{
		assert(v);
		x = v[0];
		y = v[1];
	}

	float &operator[](size_t index)
	{
		assert(index >= 0 && index <= 1);
		return (&x)[index];
	}

	const float &operator[](size_t index) const
	{
		assert(index >= 0 && index <= 1);
		return (&x)[index];
	}

	bool operator==(const vec2 &v) const
	{
		return x == v.x && y == v.y;
	}

	bool operator!=(const vec2 &v) const
	{
		return !(*this == v);
	}

	vec2 operator+(const vec2 &v) const
	{
		return vec2(x + v.x, y + v.y);
	}

	vec2 operator-(const vec2 &v) const
	{
		return vec2(x - v.x, y - v.y);
	}

	vec2 operator*(const vec2 &v) const
	{
		return vec2(x * v.x, y * v.y);
	}

	vec2 operator*(float value) const
	{
		return vec2(x * value, y * value);
	}

	vec2 operator/(float value) const
	{
		return vec2(x / value, y / value);
	}

	void operator+=(const vec2 &v)
	{
		x += v.x;
		y += v.y;
	}

	void operator-=(const vec2 &v)
	{
		x -= v.x;
		y -= v.y;
	}

	void operator*=(const vec2 &v)
	{
		x *= v.x;
		y *= v.y;
	}

	void operator*=(float value)
	{
		x *= value;
		y *= value;
	}

	static vec2 lerp(const vec2 &from, const vec2 &to, float fraction)
	{
		return vec2(math::Lerp(from.x, to.x, fraction), math::Lerp(from.y, to.y, fraction));
	}

	static const vec2 empty;

	union { float x, u; };
	union { float y, v; };
};

class vec2i
{
public:
	vec2i() : x(0), y(0) {}
	vec2i(int x, int y) : x(x), y(y) {}

	int &operator[](size_t index)
	{
		assert(index >= 0 && index <= 1);
		return (&x)[index];
	}

	const int &operator[](size_t index) const
	{
		assert(index >= 0 && index <= 1);
		return (&x)[index];
	}

	vec2i operator*(const vec2i &v) const
	{
		return vec2i(x * v.x, y * v.y);
	}

	int x, y;
};

class vec3
{
public:
	vec3() : x(0), y(0), z(0) {}

	vec3(float x, float y, float z)
	{
		this->x = x;
		this->y = y;
		this->z = z;
	}

	vec3(float d) : x(d), y(d), z(d) {}

	vec3(const float *v)
	{
		assert(v);
		x = v[0];
		y = v[1];
		z = v[2];
	}

	static float dotProduct(const vec3 &v1, const vec3 &v2);
	static vec3 crossProduct(const vec3 &v1, const vec3 &v2);
	static vec3 lerp(const vec3 &from, const vec3 &to, float fraction);
	static float distance(const vec3 &v1, const vec3 &v2);
	static float distanceSquared(const vec3 &v1, const vec3 &v2);
	static vec3 anglesSubtract(const vec3 &v1, const vec3 &v2);

	static vec3 fromBytes(const uint8_t *data)
	{
		return vec3(data[0] / 255.0f, data[1] / 255.0f, data[2] / 255.0f);
	}

	float length() const;
	float lengthSquared() const;
	vec3 absolute() const;
	vec3 normal() const;
	vec3 perpendicular() const;

	/// Create two perpendicular vectors from a normalized forward vector.
	void toNormalVectors(vec3 *right, vec3 *up) const;

	vec3 toAngles() const;
	void toAngleVectors(vec3 *forward, vec3 *right = 0, vec3 *up = 0) const;
	vec3 rotated(const vec3 &direction, float degrees) const;
	vec3 rotatedAroundDirection(vec3 direction, float degrees) const;
	vec3 inverse() const;

	void invert();
	void snap();
	void snapTowards(const vec3 &v);
	float normalize();
	void normalizeFast();

	float &operator[](size_t index)
	{
		assert(index >= 0 && index <= 2);
		return (&x)[index];
	}

	const float &operator[](size_t index) const
	{
		assert(index >= 0 && index <= 2);
		return (&x)[index];
	}

	bool equals(const vec3 &v, float epsilon = 0.001f) const
	{
		return math::FuzzyEquals(x, v.x, epsilon) && math::FuzzyEquals(y, v.y, epsilon) && math::FuzzyEquals(z, v.z, epsilon);
	}

	bool operator==(const vec3 &v) const
	{
		return equals(v);
	}

	bool operator!=(const vec3 &v) const
	{
		return !(*this == v);
	}

	vec3 operator+(const vec3 &v) const
	{
		return vec3(x + v.x, y + v.y, z + v.z);
	}

	vec3 operator-() const
	{
		return vec3(-x, -y, -z);
	}

	vec3 operator-(const vec3 &v) const
	{
		return vec3(x - v.x, y - v.y, z - v.z);
	}

	vec3 operator*(const vec3 &v) const
	{
		return vec3(x * v.x, y * v.y, z * v.z);
	}

	vec3 operator*(float value) const
	{
		return vec3(x * value, y * value, z * value);
	}

	vec3 operator/(float value) const
	{
		return vec3(x / value, y / value, z / value);
	}

	void operator+=(const vec3 &v)
	{
		x += v.x;
		y += v.y;
		z += v.z;
	}

	void operator-=(const vec3 &v)
	{
		x -= v.x;
		y -= v.y;
		z -= v.z;
	}

	void operator*=(const vec3 &v)
	{
		x *= v.x;
		y *= v.y;
		z *= v.z;
	}

	void operator*=(float value)
	{
		x *= value;
		y *= value;
		z *= value;
	}

	static const vec3 empty;

	union { float x, r; };
	union { float y, g; };
	union { float z, b; };

private:
	vec3 projectOnPlane(const vec3 &normal) const;
};

// From http://stackoverflow.com/a/9557244
inline vec3 ClosestPointOnLineSegment(vec3 A, vec3 B, vec3 P)
{
	vec3 AP = P - A;       //Vector from A to P   
    vec3 AB = B - A;       //Vector from A to B  

    float magnitudeAB = AB.length() * AB.length();     //Magnitude of AB vector (it's length squared)     
    float ABAPproduct = vec3::dotProduct(AP, AB);    //The DOT product of a_to_p and a_to_b     
    float distance = ABAPproduct / magnitudeAB; //The normalized "distance" from a to your closest point  
	return A + AB * Clamped(distance, 0.0f, 1.0f);
}

class vec3i
{
public:
	vec3i() : x(0), y(0), z(0) {}
	vec3i(int x, int y, int z) : x(x), y(y), z(z) {}

	int &operator[](size_t index)
	{
		assert(index >= 0 && index <= 2);
		return (&x)[index];
	}

	const int &operator[](size_t index) const
	{
		assert(index >= 0 && index <= 2);
		return (&x)[index];
	}

	int x, y, z;
};

class vec3b
{
public:
	vec3b() : x(0), y(0), z(0) {}
	vec3b(uint8_t x, uint8_t y, uint8_t z) : x(x), y(y), z(z) {}

	uint8_t &operator[](size_t index)
	{
		assert(index >= 0 && index <= 2);
		return (&x)[index];
	}

	const uint8_t &operator[](size_t index) const
	{
		assert(index >= 0 && index <= 2);
		return (&x)[index];
	}

	uint8_t x, y, z;
};

class vec4
{
public:
	vec4() : x(0), y(0), z(0), w(0) {}
	vec4(float uniform) { x = y = z = w = uniform; }
		
	vec4(float x, float y, float z, float w)
	{
		this->x = x;
		this->y = y;
		this->z = z;
		this->w = w;
	}

	vec4(const vec3 &v, float w = 1)
	{
		x = v[0];
		y = v[1];
		z = v[2];
		this->w = w;
	}

	vec4(const float *v)
	{
		assert(v);
		x = v[0];
		y = v[1];
		z = v[2];
		w = v[3];
	}

	const float &operator[](size_t index) const
	{
		assert(index >= 0 && index <= 4);
		return (&x)[index];
	}

	float &operator[](size_t index)
	{
		assert(index >= 0 && index <= 4);
		return (&x)[index];
	}

	vec4 operator*(float value) const
	{
		return vec4(x * value, y * value, z * value, w * value);
	}

	vec4 operator/(float value) const
	{
		return vec4(x / value, y / value, z / value, w / value);
	}

	void operator+=(const vec4 &v)
	{
		x += v.x;
		y += v.y;
		z += v.z;
		w += v.w;
	}

	void operator*=(float value)
	{
		x *= value;
		y *= value;
		z *= value;
		w *= value;
	}

	void operator/=(float value)
	{
		x /= value;
		y /= value;
		z /= value;
		w /= value;
	}

	void toBytes(uint8_t *data) const
	{
		assert(data);
		data[0] = uint8_t(x * 255.0f);
		data[1] = uint8_t(y * 255.0f);
		data[2] = uint8_t(z * 255.0f);
		data[3] = uint8_t(w * 255.0f);
	}

	bool equals(const vec4 &v, float epsilon = 0.001f) const
	{
		return math::FuzzyEquals(x, v.x, epsilon) && math::FuzzyEquals(y, v.y, epsilon) && math::FuzzyEquals(z, v.z, epsilon) && math::FuzzyEquals(w, v.w, epsilon);
	}

	vec2 xy() const { return vec2(x, y); }
	vec3 xyz() const { return vec3(x, y, z); }
	vec3 rgb() const { return vec3(x, y, z); }

	union { float x, r; };
	union { float y, g; };
	union { float z, b; };
	union { float w, a; };

	static const vec4 empty;
	static const vec4 black, red, green, blue, yellow, magenta, cyan, white, lightGrey, mediumGrey, darkGrey;

	static float dotProduct(const vec4 &v1, const vec4 &v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
	}

	static vec4 lerp(const vec4 &from, const vec4 &to, float fraction)
	{
		return vec4(math::Lerp(from.x, to.x, fraction), math::Lerp(from.y, to.y, fraction), math::Lerp(from.z, to.z, fraction), math::Lerp(from.w, to.w, fraction));
	}

	static vec4 fromBytes(const uint8_t *data)
	{
		return vec4(data[0] / 255.0f, data[1] / 255.0f, data[2] / 255.0f, data[3] / 255.0f);
	}
};

class vec4b
{
public:
	vec4b() : x(0), y(0), z(0), w(0) {}
	vec4b(uint8_t x, uint8_t y, uint8_t z, uint8_t w) : x(x), y(y), z(z), w(w) {}
	explicit vec4b(vec4 v)
	{
		x = uint8_t(v.x * 255.0f);
		y = uint8_t(v.y * 255.0f);
		z = uint8_t(v.z * 255.0f);
		w = uint8_t(v.w * 255.0f);
	}

	uint8_t &operator[](size_t index)
	{
		assert(index >= 0 && index <= 3);
		return (&x)[index];
	}

	const uint8_t &operator[](size_t index) const
	{
		assert(index >= 0 && index <= 3);
		return (&x)[index];
	}

	union { uint8_t x, r; };
	union { uint8_t y, g; };
	union { uint8_t z, b; };
	union { uint8_t w, a; };

	static vec4b lerp(vec4b from, vec4b to, float fraction)
	{
		return vec4b(math::Lerp(from.x, to.x, fraction), math::Lerp(from.y, to.y, fraction), math::Lerp(from.z, to.z, fraction), math::Lerp(from.w, to.w, fraction));
	}
};

class Bounds
{
public:
	Bounds();
	Bounds(const vec3 &min, const vec3 &max);
	Bounds(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
	Bounds(const vec3 &origin, float radius);
	float toRadius() const;
	vec3 toSize() const;
	void toVertices(vec3 *vertices) const;
	std::array<vec3, 8> toVertices() const;
	Bounds toModelSpace() const;
	bool intersectSphere(const vec3 &v, float radius) const;
	bool intersectPoint(const vec3 &v) const;
	bool intersectPoint(const vec3 &v, float epsilon) const;
	float calculateFarthestCornerDistance(const vec3 &pos) const;
	void expand(float amount);
	vec3 midpoint() const;
	void setupForAddingPoints();
	void addPoint(const vec3 &v);
	void addPoints(const vec3 *points, size_t nPoints);
	void addPoints(const Bounds &b);

	const vec3 &operator[](size_t i) const;
	vec3 &operator[](size_t i);
	Bounds operator+(const vec3 &v) const;
	bool operator==(const Bounds &b) const;

	static bool intersect(const Bounds &b1, const Bounds &b2);
	static bool intersect(const Bounds &b1, const Bounds &b2, float epsilon);
	static Bounds merge(const Bounds &b1, const Bounds &b2);

	static const Bounds empty;

	vec3 min, max;
};

class mat3
{
public:
	mat3();
	mat3(const vec3 &a1, const vec3 &a2, const vec3 &a3);
	mat3(const vec3 &angles);
	mat3(const mat4 &m);
	mat3(const float axis[3][3]);

	void transpose();
	void rotateAroundDirection(float yaw);
	float determinate() const;
	mat3 inverse() const;
	vec3 transform(const vec3 &v) const;
	vec3 &operator[](size_t index);
	const vec3 &operator[](size_t index) const;
	mat3 operator*(const mat3 &m) const;

	static mat3 rotation(float degrees, const vec3 &axis);
	static mat3 rotationX(float degrees);
	static mat3 rotationY(float degrees);
	static mat3 rotationZ(float degrees);

	static const mat3 identity;

private:
	vec3 rows_[3];
};

class mat4
{
public:
	mat4();
	mat4(const float *m);
	mat4(float e0, float e1, float e2, float e3, float e4, float e5, float e6, float e7, float e8, float e9, float e10, float e11, float e12, float e13, float e14, float e15);
	mat4(const mat3 &m);

	bool equals(const mat4 &m) const;
	Bounds transform(const Bounds &bounds) const;
	vec3 transform(const vec3 &v) const;
	vec4 transform(const vec4 &v) const;
	vec3 transformNormal(const vec3 &n) const;
	float determinate() const;
	void extract(mat3 *rotation, vec3 *translation) const;

	void copy(const mat4 &m);
	void transpose();
	void setupScale(float scale);
	void setupScale(float sx, float sy, float sz);
	void setupScale(const vec3 &v);
	void setupTransform(const mat4 &rot, const vec3 &origin);
	void setupTransform(const mat3 &rot, const vec3 &origin);
	void setupOrthographicProjection(int left, int right, int top, int bottom);
	void setupOrthographicProjection(const Bounds &bounds);
	void setupOrthographicProjection(float l, float r, float t, float b, float zn, float zf);
	void setupPerspectiveProjection(float fovX, float fovY, float zNear, float zFar);
	void invert();

	const float &operator[](size_t i) const;
	float &operator[](size_t i);
	mat4 operator*(const mat4 &m) const;
	void operator*=(const mat4 &m);

	const float *get() const { return &e_[0]; }
	float *get() { return &e_[0]; }

	static const mat4 empty;
	static const mat4 identity;
	static mat4 perspectiveProjection(float fovX, float fovY, float zNear, float zFar);
	static mat4 perspectiveProjection(float l, float r, float t, float b, float n, float f);
	static mat4 orthographicProjection(const Bounds &bounds);
	static mat4 orthographicProjection(float l, float r, float t, float b, float zn, float zf);
	static mat4 view(const vec3 &position, const mat3 &rotation);
	static mat4 lookAt(const vec3 &eye, const vec3 &direction, const vec3 &up);
	static mat4 translate(const vec3 &position);
	static mat4 scale(const vec3 &);
	static mat4 transform(const mat3 &rot, const vec3 &origin);
	static mat4 crop(const Bounds &bounds);

private:
	void calculateSubmat3x3(float *e3x3, int i, int j) const;
	float calculateDeterminate3x3(float *e3x3) const;

	float e_[16];
};

class Plane
{
public:
	enum Side
	{
		Front,
		Back,
		On
	};

	enum class Type : uint8_t
	{
		AxialX,
		AxialY,
		AxialZ,
		NonAxial
	};

	Plane();
	Plane(float a, float b, float c, float d);
	Plane(vec3 normal, float distance);

	float &operator[](size_t index)
	{
		assert(index >= 0 && index < 4);
		return index == 3 ? distance : normal[index];
	}

	const float &operator[](size_t index) const
	{
		assert(index >= 0 && index < 4);
		return index == 3 ? distance : normal[index];
	}

	float calculateDistance(const vec3 &v) const;
	Side calculateSide(const vec3 &v, const float epsilon = 0.001f) const;
	Plane inverse() const;
	void invert();

	void setupFastBoundsTest();

	// Returns 1, 2, or 1 + 2
	int testBounds(Bounds bounds);

	vec4 toVec4() const { return vec4(normal, distance); }

	vec3 normal;
	float distance;

private:
	void normalize();

	Type type_; // for fast side tests
	uint8_t signBits_; // signx + (signy<<1) + (signz<<2), used as lookup during collision
	uint8_t pad_[2];
};

class Frustum
{
public:
	enum
	{
		LeftPlane,
		RightPlane,
		TopPlane,
		BottomPlane,
		NearPlane, // Not used for culling
		FarPlane // Not used for culling
	};

	enum class ClipResult
	{
		Inside,
		Outside,
		Partial
	};

	Frustum() {}
	Frustum(const mat4 &mvp);
		
	const Plane &operator[](size_t index) const
	{
		assert(index >= 0 && index < 6);
		return planes_[index];
	}

	Plane &operator[](size_t index)
	{
		assert(index >= 0 && index < 6);
		return planes_[index];
	}

	ClipResult clipBounds(const Bounds &bounds, const mat4 &modelMatrix = mat4::identity) const;
	ClipResult clipSphere(const vec3 &position, float radius) const;
	bool isInside(const vec3 &v) const;

private:
	ClipResult clipBox(const std::array<vec3, 8> &corners) const;

	Plane planes_[6];
	static const size_t nPlanesToClipAgainst_ = 4;
};

struct Transform
{
	vec3 position;
	mat3 rotation;
};

} // namespace math
