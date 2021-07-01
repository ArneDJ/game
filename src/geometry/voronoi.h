namespace geom {

struct voronoi_cell_t;
struct voronoi_vertex_t;
struct voronoi_edge_t;

struct voronoi_edge_t {
	uint32_t index;
	const voronoi_vertex_t *v0 = nullptr;
	const voronoi_vertex_t *v1 = nullptr;
	const voronoi_cell_t *c0 = nullptr;
	const voronoi_cell_t *c1 = nullptr;
};

struct voronoi_vertex_t {
	uint32_t index;
	glm::vec2 position;
	std::vector<const voronoi_vertex_t*> adjacent;
	std::vector<const voronoi_cell_t*> cells;
};

struct voronoi_cell_t {
	uint32_t index;
	glm::vec2 center;
	std::vector<const voronoi_cell_t*> neighbors;
	std::vector<const voronoi_vertex_t*> vertices;
	std::vector<const voronoi_edge_t*> edges;
};

class Voronoi {
public:
	std::vector<voronoi_cell_t> cells;
	std::vector<voronoi_vertex_t> vertices;
	std::vector<voronoi_edge_t> edges;
public:
	void gen_diagram(std::vector<glm::vec2> &locations, glm::vec2 min, glm::vec2 max, uint8_t relaxations);
};

};
