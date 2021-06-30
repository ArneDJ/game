
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
	friend class Saver; // needs acces to internal data to save it
public:
	const glm::vec3 SCALE = { 4096.F, 200.F, 4096.F };
public:
	Atlas();
	void generate(long seedling, const struct MODULE::worldgen_parameters_t *params);
	void create_mapdata(long seed);
	void create_land_navigation();
	void create_sea_navigation();
public:
	const Terragen* get_terragen() const;
	const UTIL::Image<uint8_t>* get_watermap() const;
	const UTIL::Image<uint8_t>* get_materialmasks() const;
	const UTIL::Image<uint8_t>* get_vegetation() const;
	const UTIL::Image<uint8_t>* get_factions() const;
	const std::vector<geom::transformation_t>& get_trees() const;
	const struct navigation_soup_t& get_navsoup() const;
	const std::unordered_map<uint32_t, holding_t>& get_holdings() const;
	const std::unordered_map<uint32_t, uint32_t>& get_holding_tiles() const;
	const Worldgraph* get_worldgraph() const;
public:
	const struct tile* tile_at_position(const glm::vec2 &position) const;
public:
	void colorize_holding(uint32_t holding, const glm::vec3 &color);
private:
	std::unique_ptr<Terragen> terragen;
	std::unique_ptr<Worldgraph> worldgraph;
	UTIL::Image<uint8_t> watermap; // heightmap of ocean, seas and rivers
	UTIL::Image<uint8_t> vegetation;
	UTIL::Image<uint8_t> factions; // color map of factions
	UTIL::Image<uint8_t> materialmasks;
	UTIL::Image<float> container;
	UTIL::Image<float> detail;
	UTIL::Image<uint8_t> mask;
private:
	std::unordered_map<uint32_t, holding_t> holdings; // TODO put in worldgraph
	std::unordered_map<uint32_t, uint32_t> holding_tiles;
	Mapfield mapfield;
	struct navigation_soup_t navsoup;
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
