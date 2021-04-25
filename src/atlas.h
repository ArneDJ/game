
struct holding {
	uint32_t ID;
	std::string name;
	struct tile *center; // center tile of the hold that contains a fortification
	std::vector<struct tile*> lands; // tiles that the holding consists of
	std::vector<const struct holding*> neighbors; // neighbouring holds
};

struct navigation_soup {
	std::vector<float> vertices;
	std::vector<int> indices;
};

class Atlas {
public:
	const glm::vec3 SCALE = { 4096.F, 200.F, 4096.F };
	Worldgraph *worldgraph;
public:
	Atlas(void);
	~Atlas(void);
	void generate(long seedling, const struct worldparams *params);
	void create_mapdata(long seed);
	void create_land_navigation(void);
	void create_sea_navigation(void);
public:
	const FloatImage* get_heightmap(void) const;
	const Image* get_watermap(void) const;
	const Image* get_rainmap(void) const;
	const Image* get_tempmap(void) const;
	const Image* get_materialmasks(void) const;
	const Image* get_vegetation(void) const;
	const std::vector<Entity*>& get_trees(void) const;
	const struct navigation_soup* get_navsoup(void) const;
public:
	void load_heightmap(uint16_t width, uint16_t height, const std::vector<float> &data);
	void load_watermap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data);
	void load_rainmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data);
	void load_tempmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data);
public:
	const struct tile* tile_at_position(const glm::vec2 &position) const;
private:
	Terragen *terragen;
	Image *watermap; // heightmap of ocean, seas and rivers
	Image *vegetation;
	Image *materialmasks;
	std::vector<struct holding> holdings;
	std::unordered_map<uint32_t, uint32_t> holding_tiles;
	//void name_holds(void);
	Mapfield mapfield;
	FloatImage *container;
	FloatImage *detail;
	Image *mask;
	Image *tree_density;
	struct navigation_soup navsoup;
	std::vector<Entity*> trees;
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
	void place_vegetation(long seed);
	void clear_entities(void);
};

