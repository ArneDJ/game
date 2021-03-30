
class Landscape {
public:
	FloatImage *heightmap = nullptr;
public:
	Landscape(uint16_t heightres);
	~Landscape(void);
	void generate(long seed, const glm::vec2 &offset);
private:
	void gen_heightmap(long seed, const glm::vec2 &offset);
};
