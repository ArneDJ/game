
struct rectangle {
	glm::vec2 min;
	glm::vec2 max;
};

bool clockwise(glm::vec2 a, glm::vec2 b, glm::vec2 c);

int orient(float x0, float y0, float x1, float y1, float x2, float y2);

glm::vec2 segment_midpoint(const glm::vec2 &a, const glm::vec2 &b);
