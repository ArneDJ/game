
struct model_info {
	std::string model;
	glm::vec3 bounds;
};

struct building_t {
	std::string model;
	glm::vec3 bounds;
	std::vector<Entity*> entities;
};

class Landscape {
public:
	glm::vec3 SCALE = { 6144.F, 512.F, 6144.F };
public:
	Landscape(uint16_t heightres, const std::vector<struct model_info> &house_templates);
	~Landscape(void);
	void clear(void);
	void generate(long campaign_seed, uint32_t tileref, int32_t local_seed, float amplitude, uint8_t precipitation, uint8_t site_radius, bool walled);
public:
	const FloatImage* get_heightmap(void) const;
	const Image* get_normalmap(void) const;
	const std::vector<Entity*>& get_trees(void) const;
	const std::vector<building_t>& get_houses(void) const;
	float sample_heightmap(const glm::vec2 &real) const;
private:
	FloatImage *heightmap = nullptr;
	FloatImage *container = nullptr;
	Image *normalmap = nullptr;
	Image *density = nullptr;
	Poisson poisson;
	Sitegen sitegen;
private:
	std::vector<Entity*> trees;
	std::vector<building_t> houses;
private:
	void gen_heightmap(long campaign_seed, int32_t local_seed, float amplitude);
	void gen_forest(int32_t seed, uint8_t precipitation);
	void place_houses(bool walled);
};
