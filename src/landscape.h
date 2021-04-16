
class Landscape {
public:
	glm::vec3 SCALE = { 6144.F, 512.F, 6144.F };
public:
	Landscape(uint16_t heightres);
	~Landscape(void);
	void generate(long seed, uint32_t offset, float amplitude);
	const FloatImage* get_heightmap(void) const;
	const Image* get_normalmap(void) const;
private:
	FloatImage *heightmap = nullptr;
	FloatImage *container = nullptr;
	Image *normalmap = nullptr;
private:
	void gen_heightmap(long seed, uint32_t offset, float amplitude);
};
