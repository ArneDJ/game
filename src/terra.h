
class Terragen {
public:
	FloatImage *heightmap = nullptr;
	Image *tempmap = nullptr;
	Image *rainmap = nullptr;
public:
	Terragen(uint16_t heightres, uint16_t rainres, uint16_t tempres);
	~Terragen(void);
	void generate(int64_t seed, const struct worldparams *params);
private:
	void gen_heightmap(int64_t seed, const struct worldparams *params);
	void gen_tempmap(int64_t seed, const struct worldparams *params);
	void gen_rainmap(int64_t seed, const struct worldparams *params);
};
