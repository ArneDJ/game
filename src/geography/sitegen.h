struct district;
struct junction;
struct section;

struct parcel {
	geom::quadrilateral_t quad;
	glm::vec2 centroid;
	glm::vec2 direction; // normalized direction vector to the closest street, if a parcel has a building on it it will be rotated in that direction
};

struct wallsegment {
	const struct district *left = nullptr;
	const struct district *right = nullptr;
	geom::segment_t S;
	bool gate;
};

struct section {
	uint32_t index;
	struct junction *j0 = nullptr;
	struct junction *j1 = nullptr;
	struct district *d0 = nullptr;
	struct district *d1 = nullptr;
	bool border;
	bool wall; // a wall goes through here
	float area;
	bool gateway;
};

struct junction {
	uint32_t index;
	glm::vec2 position;
	std::vector<struct junction*> adjacent;
	std::vector<struct district*> districts;
	bool border;
	int radius;
	bool street;
};

struct district {
	uint32_t index;
	glm::vec2 center;
	std::vector<struct district*> neighbors;
	std::vector<struct junction*> junctions;
	std::vector<struct section*> sections;
	bool border;
	int radius; // distance to center in graph structure
	float area;
	bool tower;
	glm::vec2 centroid;
	std::vector<struct parcel> parcels;
};

class Sitegen {
public:
	struct district *core;
	long seed;
	std::vector<struct wallsegment> walls;
	std::vector<geom::segment_t> highways;
	// graph structures
	std::vector<struct district> districts;
	std::vector<struct junction> junctions;
	std::vector<struct section> sections;
public:
	void generate(long seedling, uint32_t tileref, geom::rectangle_t bounds, size_t wall_radius);
private:
	geom::rectangle_t area;
private:
	void make_diagram(uint32_t tileref);
	void mark_districts(void);
	void mark_junctions(void);
	void outline_walls(size_t radius);
	void make_gateways(void);
	void finalize_walls(void);
	void make_highways(void);
	void divide_parcels(size_t radius);
};

