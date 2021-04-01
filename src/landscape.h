
class Landscape {
public:
	FloatImage *heightmap = nullptr;
public:
	Landscape(uint16_t heightres);
	~Landscape(void);
	void generate(long seed, float amplitude);
private:
	void gen_heightmap(long seed, float amplitude);
};
