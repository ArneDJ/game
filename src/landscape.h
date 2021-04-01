
class Landscape {
public:
	FloatImage *heightmap = nullptr;
	Image *normalmap = nullptr;
public:
	Landscape(uint16_t heightres);
	~Landscape(void);
	void generate(long seed, uint32_t offset, float amplitude);
private:
	FloatImage *container = nullptr;
private:
	void gen_heightmap(long seed, uint32_t offset, float amplitude);
};
