struct tile;
struct corner;
struct border;

enum SITE : uint8_t {
	VACANT,
	RESOURCE,
	TOWN,
	CASTLE,
	RUIN
};

enum RELIEF : uint8_t {
	SEABED,
	LOWLAND,
	UPLAND,
	HIGHLAND
};

enum BIOME : uint8_t {
	SEA,
	GLACIER,
	DESERT,
	SAVANNA,
	SHRUBLAND,
	STEPPE,
	BROADLEAF_GRASSLAND,
	BROADLEAF_FOREST,
	PINE_GRASSLAND,
	PINE_FOREST,
	FLOODPLAIN,
	BADLANDS
};

struct border {
	uint32_t index;
	// world data
	bool frontier;
	bool coast;
	bool river;
	bool wall;
	// graph data
	struct corner *c0 = nullptr;
	struct corner *c1 = nullptr;
	struct tile *t0 = nullptr;
	struct tile *t1 = nullptr;
};

struct corner {
	// graph data
	uint32_t index;
	// world data
	bool frontier;
	bool coast;
	bool river;
	bool wall;
	// graph data
	glm::vec2 position;
	std::vector<struct corner*> adjacent;
	std::vector<struct tile*> touches;
	int depth;
};

struct tile {
	// graph data
	uint32_t index;
	// world data
	bool frontier;
	bool land;
	bool coast;
	bool river;
	// graph data
	glm::vec2 center;
	std::vector<const struct tile*> neighbors;
	std::vector<const struct corner*> corners;
	std::vector<const struct border*> borders;
	//
	float amp;
	enum RELIEF relief;
	enum BIOME biome;
	enum SITE site;
	struct holding *hold = nullptr;
};

struct branch {
	const struct corner *confluence = nullptr;
	struct branch *left = nullptr;
	struct branch *right = nullptr;
	int streamorder;
	int depth;
};

struct basin {
	struct branch *mouth; // binary tree root
	size_t height; // binary tree height
};

struct holding {
	int index;
	std::string name;
	struct tile *center; // center tile of the hold that contains a fortification
	std::vector<struct tile*> lands; // tiles that the holding consists of
	std::vector<const struct holding*> neighbors; // neighbouring holds
};

class Worldgraph {
public:
	struct rectangle area;
	std::vector<struct tile> tiles;
	std::vector<struct corner> corners;
	std::vector<struct border> borders;
	std::list<struct holding> holdings;
	long seed;
public:
	Worldgraph(const struct rectangle bounds);
	~Worldgraph(void);
	void generate(long seedling, const struct worldparams *params, const Terragen *terra);
private:
	std::list<struct basin> basins;
	Voronoi voronoi;
private:
	void gen_diagram(void);
	void gen_relief(const FloatImage *heightmap, const struct worldparams *params);
	void gen_rivers(bool erodable);
	void gen_biomes(const Image *tempmap, const Image *rainmap);
	void gen_sites(void);
	void gen_holds(void);
	void name_holds(void);
	void floodfill_relief(unsigned int minsize, enum RELIEF target, enum RELIEF replacement);
	void remove_echoriads(void);
	void gen_drainage_basins(std::vector<const struct corner*> &graph);
	void erode_mountains(void);
	void correct_border_rivers(void);
	void find_obstructions(void);
	void correct_walls(void);
	//
	void trim_river_basins(void);
	void trim_stubby_rivers(void);
	void prune_basins(void);
};
