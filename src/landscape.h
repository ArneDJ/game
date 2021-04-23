
class Landscape {
public:
	glm::vec3 SCALE = { 6144.F, 512.F, 6144.F };
public:
	Landscape(uint16_t heightres);
	~Landscape(void);
	void clear(void);
	void generate(long campaign_seed, int32_t local_seed, float amplitude, uint8_t precipitation);
	const FloatImage* get_heightmap(void) const;
	const Image* get_normalmap(void) const;
	const std::vector<Entity*>& get_trees(void) const;
private:
	FloatImage *heightmap = nullptr;
	FloatImage *container = nullptr;
	Image *normalmap = nullptr;
	Image *density = nullptr;
	Poisson poisson;
private:
	std::vector<Entity*> trees;
private:
	void gen_heightmap(long campaign_seed, int32_t local_seed, float amplitude);
	void gen_forest(int32_t seed, uint8_t precipitation);
};
