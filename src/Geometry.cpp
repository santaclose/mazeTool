#include "Geometry.h"
#include <cmath>

float Geometry::cross2D(const vec& a, const vec& b)
{
	return a.x * b.z - a.z * b.x;
}
float Geometry::dot2D(const vec& a, const vec& b)
{
	return a.x * b.x + a.z * b.z;
}

vec Geometry::intersectLines(const vec& pointA, const vec& pointB, const vec& dirA, const vec& dirB, float threshold)
{
	if (abs(dot2D(dirA.Normalized(), dirB.Normalized())) > threshold)
	{
		return (pointA + pointB) * 0.5;
	}

#define p pointA
#define q pointB
#define r dirA
#define s dirB
	vec qp = q - p;
	float rs = cross2D(r, s);

	vec result;
	if (rs != 0.0 && cross2D(qp, r) != 0.0)
	{
		float t = cross2D(qp, s) / rs;
		float u = cross2D(qp, r) / rs;
		result = (p + (r * t) + q + (s * u)) * 0.5;
	}
	else
	{
		result = (p + q) * 0.5;
	}
	return result;
#undef p
#undef q
#undef r
#undef s
}