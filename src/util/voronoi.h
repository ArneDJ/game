namespace UTIL {

struct voronoi_cell;
struct voronoi_vertex;
struct voronoi_edge;

struct voronoi_edge {
	uint32_t index;
	const struct voronoi_vertex *v0 = nullptr;
	const struct voronoi_vertex *v1 = nullptr;
	const struct voronoi_cell *c0 = nullptr;
	const struct voronoi_cell *c1 = nullptr;
};

struct voronoi_vertex {
	uint32_t index;
	glm::vec2 position;
	std::vector<const struct voronoi_vertex*> adjacent;
	std::vector<const struct voronoi_cell*> cells;
};

struct voronoi_cell {
	uint32_t index;
	glm::vec2 center;
	std::vector<const struct voronoi_cell*> neighbors;
	std::vector<const struct voronoi_vertex*> vertices;
	std::vector<const struct voronoi_edge*> edges;
};

class Voronoi {
public:
	std::vector<struct voronoi_cell> cells;
	std::vector<struct voronoi_vertex> vertices;
	std::vector<struct voronoi_edge> edges;
public:
	void gen_diagram(std::vector<glm::vec2> &locations, glm::vec2 min, glm::vec2 max, uint8_t relaxations);
};

};
