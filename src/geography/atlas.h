
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
	Atlas(void);
	~Atlas(void);
	void generate(long seedling, const struct worldparams *params);
	void create_mapdata(long seed);
	void create_land_navigation(void);
	void create_sea_navigation(void);
public:
	const UTIL::Image<float>* get_heightmap(void) const;
	const UTIL::Image<uint8_t>* get_watermap(void) const;
	const UTIL::Image<uint8_t>* get_rainmap(void) const;
	const UTIL::Image<uint8_t>* get_tempmap(void) const;
	const UTIL::Image<uint8_t>* get_materialmasks(void) const;
	const UTIL::Image<uint8_t>* get_vegetation(void) const;
	const UTIL::Image<uint8_t>* get_factions(void) const;
	const std::vector<transformation>& get_trees(void) const;
	const struct navigation_soup_t& get_navsoup(void) const;
	const std::unordered_map<uint32_t, holding_t>& get_holdings(void) const;
	const std::unordered_map<uint32_t, uint32_t>& get_holding_tiles(void) const;
	const Worldgraph* get_worldgraph(void) const;
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
	UTIL::Image<uint8_t> tree_density;
private:
	std::unordered_map<uint32_t, holding_t> holdings; // TODO put in worldgraph
	std::unordered_map<uint32_t, uint32_t> holding_tiles;
	Mapfield mapfield;
	struct navigation_soup_t navsoup;
	std::vector<transformation> trees;
private:
	void gen_holds(void);
	void smoothe_heightmap(void);
	void plateau_heightmap(void);
	void oregony_heightmap(long seed);
	void gen_mapfield(void);
	void create_watermap(float ocean_level);
	void erode_heightmap(float ocean_level);
	void clamp_heightmap(float land_level);
	void create_materialmasks(void);
	void create_vegetation(void);
	void create_factions_map(void);
	void place_vegetation(long seed);
	void clear_entities(void);
};
