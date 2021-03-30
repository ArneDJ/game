
struct rectangle {
	glm::vec2 min;
	glm::vec2 max;
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

