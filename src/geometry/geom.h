
namespace geom {

struct transformation_t {
	glm::vec3 position = {};
	glm::quat rotation = {};
	float scale = 1.f;
};

struct rectangle_t {
	glm::vec2 min = {};
	glm::vec2 max = {};
};

/*
 * A --- D
 * |     |
 * |     |
 * B --- C
 */
struct quadrilateral_t {
	glm::vec2 a, b, c, d;
};

struct segment_t {
	glm::vec2 P0, P1;
};

struct AABB_old {
	glm::vec3 c; // center point of AABB
	glm::vec3 r; // radius or halfwidth extents (rx, ry, rz)
};

struct AABB_t {
	glm::vec3 min;
	glm::vec3 max;
};

struct sphere_t {
	glm::vec3 center;
	float radius;
};

struct segment_intersection_t {
	bool intersects = false;
	glm::vec2 point = {};
};

// in this game if something in 3D is translated to 2D the 3D XZ axis becomes the 2D XY axis
inline glm::vec2 translate_3D_to_2D(const glm::vec3 &v)
{
	return glm::vec2(v.x, v.z);
}

bool clockwise(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c);

int orient(float x0, float y0, float x1, float y1, float x2, float y2);

glm::vec2 segment_midpoint(const glm::vec2 &a, const glm::vec2 &b);

bool point_in_circle(const glm::vec2 &point, const glm::vec2 &origin, float radius);

bool triangle_overlaps_point(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &pt);

float triangle_area(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c);

bool segment_intersects_segment(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &d);

void frustum_to_planes(const glm::mat4 &M, glm::vec4 planes[6]);

bool AABB_in_frustum(const glm::vec3 &min, const glm::vec3 &max, glm::vec4 frustum_planes[6]);

glm::vec2 quadrilateral_centroid(const quadrilateral_t &quad);

segment_intersection_t segment_segment_intersection(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &d);

glm::vec2 closest_point_segment(const glm::vec2 &c, const glm::vec2 &a, const glm::vec2 &b);

bool quad_quad_intersection(const quadrilateral_t &A, const quadrilateral_t &B);

bool convex_quadrilateral(const quadrilateral_t &quad);

bool point_in_rectangle(const glm::vec2 &p, const rectangle_t &r);

bool sphere_intersects_AABB(const sphere_t &sphere, const AABB_t &aabb);

};
