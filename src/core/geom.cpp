#include <algorithm>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "geom.h"

static inline float sign(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c);

bool clockwise(glm::vec2 a, glm::vec2 b, glm::vec2 c)
{
	int wise = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);

	if (wise < 0) {
		return true;
	}
		
	return false;
}

// http://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/
int orient(float x0, float y0, float x1, float y1, float x2, float y2)
{
	return (int(x1) - int(x0))*(int(y2) - int(y0)) - (int(y1) - int(y0))*(int(x2) - int(x0));
}

glm::vec2 segment_midpoint(const glm::vec2 &a, const glm::vec2 &b)
{
	return glm::vec2(0.5f * (a.x + b.x), 0.5f * (a.y + b.y));
}

bool point_in_circle(const glm::vec2 &point, const glm::vec2 &origin, float radius)
{
	return ((point.x - origin.x) * (point.x - origin.x)) + ((point.y - origin.y) * (point.y - origin.y)) <= (radius * radius);
}

bool triangle_overlaps_point(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &pt)
{
	// barycentric coordinates
	int w0 = orient(b.x, b.y, c.x, c.y, pt.x, pt.y);
	int w1 = orient(c.x, c.y, a.x, a.y, pt.x, pt.y);
	int w2 = orient(a.x, a.y, b.x, b.y, pt.x, pt.y);

	return (w0 >= 0 && w1 >= 0 && w2 >= 0);
}

bool segment_intersects_segment(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &d)
{
	// Sign of areas correspond to which side of ab points c and d are
	float a1 = sign(a, b, d); // Compute winding of abd (+ or -)
	float a2 = sign(a, b, c); // To intersect, must have sign opposite of a1

	// If c and d are on different sides of ab, areas have different signs
	if ((a1 * a2) < 0.f) {
		// Compute signs for a and b with respect to segment cd
		float a3 = sign(c, d, a);
		// Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		float a4 = a3 + a2 - a1;
		// Points a and b on different sides of cd if areas have different signs
		if (a3 * a4 < 0.0f) { return true; }
	}

	// Segments not intersecting (or collinear)
	return false;
}

// Returns 2 times the signed triangle area. The result is positive if
// abc is ccw, negative if abc is cw, zero if abc is degenerate.
static inline float sign(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c)
{
	return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
}
