
struct building_t {
	const GRAPHICS::Model *model;
	glm::vec3 bounds;
	std::vector<struct transformation> transforms;
};

class Landscape {
public:
	glm::vec3 SCALE = { 6144.F, 512.F, 6144.F };
	const struct rectangle SITE_BOUNDS = {
		{ 2048.F, 2048.F },
		{ 4096.F, 4096.F }
	};
public:
	Landscape(uint16_t heightres);
	~Landscape(void);
public:
	void load_buildings(const std::vector<const GRAPHICS::Model*> &house_models);
public:
	void clear(void);
	void generate(long campaign_seed, uint32_t tileref, int32_t local_seed, float amplitude, uint8_t precipitation, uint8_t site_radius, bool walled, bool nautical);
public:
	const UTIL::Image<float>* get_heightmap(void) const;
	const UTIL::Image<uint8_t>* get_normalmap(void) const;
	const UTIL::Image<uint8_t>* get_sitemasks(void) const;
	const std::vector<transformation>& get_trees(void) const;
	const std::vector<building_t>& get_houses(void) const;
	float sample_heightmap(const glm::vec2 &real) const;
private:
	UTIL::Image<float> heightmap;
	UTIL::Image<float> container;
	UTIL::Image<uint8_t> valleymap;
private:
	UTIL::Image<uint8_t> normalmap;
	UTIL::Image<uint8_t> density;
	UTIL::Image<uint8_t> sitemasks;
	Sitegen sitegen;
private:
	std::vector<transformation> trees;
	std::vector<building_t> houses;
private:
	void gen_heightmap(int32_t local_seed, float amplitude);
	void gen_forest(int32_t seed, uint8_t precipitation);
	void place_houses(bool walled, uint8_t radius, int32_t seed);
	void create_sitemasks(uint8_t radius);
	void create_valleymap(void);
};
