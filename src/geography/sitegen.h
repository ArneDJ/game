struct district;
struct junction;
struct section;

struct parcel {
	geom::quadrilateral_t quad;
	glm::vec2 centroid;
	glm::vec2 direction; // normalized direction vector to the closest street, if a parcel has a building on it it will be rotated in that direction
};

struct wallsegment {
	const district *left = nullptr;
	const district *right = nullptr;
	geom::segment_t S;
	bool gate;
};

struct section {
	uint32_t index;
	junction *j0 = nullptr;
	junction *j1 = nullptr;
	district *d0 = nullptr;
	district *d1 = nullptr;
	bool border;
	bool wall; // a wall goes through here
	float area;
	bool gateway;
};

struct junction {
	uint32_t index;
	glm::vec2 position;
	std::vector<junction*> adjacent;
	std::vector<district*> districts;
	bool border;
	int radius;
	bool street;
};

struct district {
	uint32_t index;
	glm::vec2 center;
	std::vector<district*> neighbors;
	std::vector<junction*> junctions;
	std::vector<section*> sections;
	bool border;
	int radius; // distance to center in graph structure
	float area;
	bool tower;
	glm::vec2 centroid;
	std::vector<parcel> parcels;
};

class Sitegen {
public:
	district *core;
	long seed;
	std::vector<wallsegment> walls;
	std::vector<geom::segment_t> highways;
	// graph structures
	std::vector<district> districts;
	std::vector<junction> junctions;
	std::vector<section> sections;
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

