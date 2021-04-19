
struct rectangle {
	glm::vec2 min;
	glm::vec2 max;
};

struct segment {
	glm::vec2 P0, P1;
};

struct AABB {
	glm::vec3 c; // center point of AABB
	glm::vec3 r; // radius or halfwidth extents (rx, ry, rz)
};

// in this game if something in 3D is translated to 2D the 3D XZ axis becomes the 2D XY axis
inline glm::vec2 translate_3D_to_2D(const glm::vec3 &v)
{
	return glm::vec2(v.x, v.z);
}

bool clockwise(glm::vec2 a, glm::vec2 b, glm::vec2 c);

int orient(float x0, float y0, float x1, float y1, float x2, float y2);

glm::vec2 segment_midpoint(const glm::vec2 &a, const glm::vec2 &b);

bool point_in_circle(const glm::vec2 &point, const glm::vec2 &origin, float radius);

bool triangle_overlaps_point(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &pt);

bool segment_intersects_segment(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &d);
