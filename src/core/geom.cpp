#include <algorithm>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "geom.h"

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
