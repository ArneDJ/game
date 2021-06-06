
class Terragen {
public:
	UTIL::Image<float> heightmap;
	UTIL::Image<uint8_t> tempmap;
	UTIL::Image<uint8_t> rainmap;
public:
	Terragen(uint16_t heightres, uint16_t rainres, uint16_t tempres);
	void generate(long seed, const struct worldparams *params);
private:
	void gen_heightmap(long seed, const struct worldparams *params);
	void gen_tempmap(long seed, const struct worldparams *params);
	void gen_rainmap(long seed, const struct worldparams *params);
};
