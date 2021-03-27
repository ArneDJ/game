#pragma once
#include "cereal/types/vector.hpp"
#include "cereal/types/memory.hpp"

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
	// to store in save file
	std::vector<uint32_t> neighborIDs;
	std::vector<uint32_t> cornerIDs;
	std::vector<uint32_t> borderIDs;
	//
	float amp;
	enum RELIEF relief;
	enum BIOME biome;
	enum SITE site;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(index, frontier, land, coast, river, center.x, center.y, neighborIDs, cornerIDs, borderIDs, amp, relief, biome, site);
	}
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

class Worldgraph {
public:
	struct rectangle area;
	std::vector<struct tile> tiles;
	std::vector<struct corner> corners;
	std::vector<struct border> borders;
	//std::list<struct holding> holdings;
public:
	Worldgraph(const struct rectangle bounds);
	~Worldgraph(void);
	void generate(long seedling, const struct worldparams *params, const Terragen *terra);
	void reload_references(void);
private:
	std::list<struct basin> basins;
	Voronoi voronoi;
private:
	void gen_diagram(long seed);
	void gen_relief(const FloatImage *heightmap, const struct worldparams *params);
	void gen_rivers(bool erodable);
	void gen_biomes(long seed, const Image *tempmap, const Image *rainmap);
	void gen_sites(long seed);
	//void gen_holds(void);
	//void name_holds(void);
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
