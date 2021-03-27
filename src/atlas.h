
class Atlas {
public:
	const glm::vec3 scale = {4096.F, 200.F, 4096.F};
	long seed;
	Worldgraph *worldgraph;
public:
	Atlas(uint16_t heightres, uint16_t rainres, uint16_t tempres);
	~Atlas(void);
	void generate(long seedling, const struct worldparams *params);
	void create_maps(void);
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
	//Mosaicfield mosaicfield;
};

