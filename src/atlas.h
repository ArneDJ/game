
struct holding {
	uint32_t ID;
	std::string name;
	struct tile *center; // center tile of the hold that contains a fortification
	std::vector<struct tile*> lands; // tiles that the holding consists of
	std::vector<const struct holding*> neighbors; // neighbouring holds
};

class Atlas {
public:
	const glm::vec3 SCALE = { 4096.F, 200.F, 4096.F };
	Worldgraph *worldgraph;
	std::vector<float> vertex_soup;
	std::vector<int> index_soup;
public:
	Atlas(uint16_t heightres, uint16_t rainres, uint16_t tempres);
	~Atlas(void);
	void generate(long seedling, const struct worldparams *params);
	void create_mapdata(void);
	void create_land_navigation(void);
	const FloatImage* get_heightmap(void) const;
	const FloatImage* get_watermap(void) const;
	const Image* get_rainmap(void) const;
	const Image* get_tempmap(void) const;
	void load_heightmap(uint16_t width, uint16_t height, const std::vector<float> &data);
	void load_watermap(uint16_t width, uint16_t height, const std::vector<float> &data);
	void load_rainmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data);
	void load_tempmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data);
	const struct tile* tile_at_position(const glm::vec2 &position);
private:
	Terragen *terragen;
	FloatImage *watermap; // heightmap of ocean, seas and rivers
	//Image *watermask;
	std::vector<struct holding> holdings;
	std::unordered_map<uint32_t, uint32_t> holding_tiles;
	//void name_holds(void);
	Mapfield mapfield;
	FloatImage *container;
	FloatImage *detail;
	Image *mask;
private:
	void gen_holds(void);
	void smoothe_heightmap(void);
	void plateau_heightmap(void);
	void detail_heightmap(long seed);
	void gen_mapfield(void);
	void create_watermap(float ocean_level);
	void erode_heightmap(float ocean_level);
};

