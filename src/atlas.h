
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
	void create_maps(void);
	void create_land_navigation(void);
	const FloatImage* get_heightmap(void) const;
	const Image* get_rainmap(void) const;
	const Image* get_tempmap(void) const;
	const Image* get_relief(void) const;
	const Image* get_biomes(void) const;
	void load_heightmap(uint16_t width, uint16_t height, const std::vector<float> &data);
	void load_rainmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data);
	void load_tempmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data);
private:
	Terragen *terragen;
	Image *relief;
	Image *biomes;
	std::vector<struct holding> holdings;
	std::unordered_map<uint32_t, uint32_t> holding_tiles;
	//void name_holds(void);
	//Mosaicfield mosaicfield;
private:
	void gen_holds(void);
};

