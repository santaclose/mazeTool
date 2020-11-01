#pragma once
#include <modelTool/vl.h>

namespace Geometry {
	float cross2D(const vec& a, const vec& b);
	float dot2D(const vec& a, const vec& b);
	vec intersectLines(const vec& pointA, const vec& pointB, const vec& dirA, const vec& dirB, float threshold = 0.95f);
}