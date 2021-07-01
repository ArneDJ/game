namespace geography {

// graph structure of the holding
struct holding_t {
	uint32_t ID;
	uint32_t center; // center tile of the hold that contains a fortification
	std::vector<uint32_t> lands; // tiles that the holding consists of
	std::vector<uint32_t> neighbors; // neighbouring holds

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(ID, center, lands, neighbors);
	}
};

struct navigation_soup_t {
	std::vector<float> vertices;
	std::vector<int> indices;
};

class Atlas {
public:
	const glm::vec3 SCALE = { 4096.F, 200.F, 4096.F };
public:
	Atlas();
	void generate(long seedling, const module::worldgen_parameters_t *params);
	void create_mapdata(long seed);
	void create_land_navigation();
	void create_sea_navigation();
public:
	const Terragen* get_terragen() const;
	const util::Image<uint8_t>* get_watermap() const;
	const util::Image<uint8_t>* get_materialmasks() const;
	const util::Image<uint8_t>* get_vegetation() const;
	const util::Image<uint8_t>* get_factions() const;
	const std::vector<geom::transformation_t>& get_trees() const;
	const navigation_soup_t& get_navsoup() const;
	const std::unordered_map<uint32_t, holding_t>& get_holdings() const;
	const std::unordered_map<uint32_t, uint32_t>& get_holding_tiles() const;
	const Worldgraph* get_worldgraph() const;
	Worldgraph* get_worldgraph();
public:
	const tile_t* tile_at_position(const glm::vec2 &position) const;
public:
	void colorize_holding(uint32_t holding, const glm::vec3 &color);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			terragen->heightmap, 
			terragen->rainmap, 
			terragen->tempmap,
			terragen->forestation,
			watermap,
			holdings,
			worldgraph->tiles,
			worldgraph->corners,
			worldgraph->borders
		);
	}
private:
	std::unique_ptr<Terragen> terragen;
	std::unique_ptr<Worldgraph> worldgraph;
	util::Image<uint8_t> watermap; // heightmap of ocean, seas and rivers
	util::Image<uint8_t> vegetation;
	util::Image<uint8_t> factions; // color map of factions
	util::Image<uint8_t> materialmasks;
	util::Image<float> container;
	util::Image<float> detail;
	util::Image<uint8_t> mask;
private:
	std::unordered_map<uint32_t, holding_t> holdings;
	std::unordered_map<uint32_t, uint32_t> holding_tiles;
	Mapfield mapfield;
	navigation_soup_t navsoup;
	std::vector<geom::transformation_t> trees;
private:
	void gen_holds();
	void smoothe_heightmap();
	void plateau_heightmap();
	void oregony_heightmap(long seed);
	void gen_mapfield();
	void create_watermap(float ocean_level);
	void erode_heightmap(float ocean_level);
	void clamp_heightmap(float land_level);
	void create_materialmasks();
	void create_vegetation(long seed);
	void create_factions_map();
	void place_vegetation(long seed);
	void clear_entities();
};

};
