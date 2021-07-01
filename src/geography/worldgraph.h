namespace geography {

struct tile_t;
struct corner_t;
struct border_t;

enum RELIEF : uint8_t {
	SEABED,
	LOWLAND,
	UPLAND,
	HIGHLAND
};

enum class tile_regolith : uint8_t {
	STONE,
	SAND,
	SNOW,
	GRASS
};

enum class tile_feature : uint8_t {
	NONE,
	WOODS,
	FLOODPLAIN,
	RESOURCE,
	SETTLEMENT
};

struct border_t {
	uint32_t index;
	// world data
	bool frontier;
	bool coast;
	bool river;
	bool wall;
	// graph data
	corner_t *c0 = nullptr;
	corner_t *c1 = nullptr;
	tile_t *t0 = nullptr;
	tile_t *t1 = nullptr;
	// save
	uint32_t c0ID;
	uint32_t c1ID;
	uint32_t t0ID;
	uint32_t t1ID;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(index, c0ID, c1ID, t0ID, t1ID, frontier, coast, river, wall);
	}
};

struct corner_t {
	// graph data
	uint32_t index;
	// world data
	bool frontier;
	bool coast;
	bool river;
	bool wall;
	// graph data
	glm::vec2 position;
	std::vector<corner_t*> adjacent;
	std::vector<tile_t*> touches;
	// save
	std::vector<uint32_t> adjacentIDs;
	std::vector<uint32_t> touchesIDs;

	int depth;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(index, position.x, position.y, adjacentIDs, touchesIDs, frontier, coast, river, wall, depth);
	}
};

struct tile_t {
	// graph data
	uint32_t index;
	// world data
	bool frontier;
	bool land;
	bool coast;
	bool river;
	// graph data
	glm::vec2 center;
	std::vector<const tile_t*> neighbors;
	std::vector<const corner_t*> corners;
	std::vector<const border_t*> borders;
	// to store in save file
	std::vector<uint32_t> neighborIDs;
	std::vector<uint32_t> cornerIDs;
	std::vector<uint32_t> borderIDs;
	//
	float amp;
	uint8_t precipitation;
	uint8_t temperature;
	enum RELIEF relief;
	enum tile_regolith regolith = tile_regolith::SAND;
	enum tile_feature feature = tile_feature::NONE;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(index, frontier, land, coast, river, center.x, center.y, neighborIDs, cornerIDs, borderIDs, amp, precipitation, temperature, relief, regolith, feature);
	}
};

struct branch_t {
	const corner_t *confluence = nullptr;
	branch_t *left = nullptr;
	branch_t *right = nullptr;
	int streamorder;
	int depth;
};

struct basin_t {
	branch_t *mouth; // binary tree root
	size_t height; // binary tree height
};

class Worldgraph {
public:
	geom::rectangle_t area;
	std::vector<tile_t> tiles;
	std::vector<corner_t> corners;
	std::vector<border_t> borders;
public:
	Worldgraph(const geom::rectangle_t bounds);
	~Worldgraph(void);
	void generate(long seedling, const module::worldgen_parameters_t *params, const Terragen *terra);
	void reload_references(void);
private:
	std::list<basin_t> basins;
	geom::Voronoi voronoi;
private:
	void gen_diagram(long seed, float radius);
	void gen_relief(const util::Image<float> *heightmap, const module::worldgen_parameters_t *params);
	void gen_rivers(const module::worldgen_parameters_t *params);
	void gen_sites(long seed, const module::worldgen_parameters_t *params);
	void gen_properties(const util::Image<uint8_t> *temperatures, const util::Image<uint8_t> *rainfall);
	void add_primitive_features(const util::Image<uint8_t> *forestation, const util::Image<uint8_t> *rainfall);
	//
	void floodfill_relief(unsigned int minsize, enum RELIEF target, enum RELIEF replacement);
	void remove_echoriads(void);
	void gen_drainage_basins(std::vector<const corner_t*> &graph);
	void erode_mountains(void);
	void correct_border_rivers(void);
	void find_obstructions(void);
	void correct_walls(void);
	//
	void trim_river_basins(uint8_t min_stream);
	void trim_stubby_rivers(uint8_t min_branch, uint8_t min_basin);
	void prune_basins(void);
};

};
