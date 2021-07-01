namespace geography {

struct district_t;
struct junction_t;
struct section_t;

struct parcel_t {
	geom::quadrilateral_t quad;
	glm::vec2 centroid;
	glm::vec2 direction; // normalized direction vector to the closest street, if a parcel has a building on it it will be rotated in that direction
};

struct wall_segment_t {
	const district_t *left = nullptr;
	const district_t *right = nullptr;
	geom::segment_t S;
	bool gate;
};

struct section_t {
	uint32_t index;
	junction_t *j0 = nullptr;
	junction_t *j1 = nullptr;
	district_t *d0 = nullptr;
	district_t *d1 = nullptr;
	bool border;
	bool wall; // a wall goes through here
	float area;
	bool gateway;
};

struct junction_t {
	uint32_t index;
	glm::vec2 position;
	std::vector<junction_t*> adjacent;
	std::vector<district_t*> districts;
	bool border;
	int radius;
	bool street;
};

struct district_t {
	uint32_t index;
	glm::vec2 center;
	std::vector<district_t*> neighbors;
	std::vector<junction_t*> junctions;
	std::vector<section_t*> sections;
	bool border;
	int radius; // distance to center in graph structure
	float area;
	bool tower;
	glm::vec2 centroid;
	std::vector<parcel_t> parcels;
};

class Sitegen {
public:
	district_t *core;
	long seed;
	std::vector<wall_segment_t> walls;
	std::vector<geom::segment_t> highways;
	// graph structures
	std::vector<district_t> districts;
	std::vector<junction_t> junctions;
	std::vector<section_t> sections;
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

};
