
class Terragen {
public:
	UTIL::Image<float> heightmap;
	UTIL::Image<uint8_t> volcanism; // TODO add vegetation map
	UTIL::Image<uint8_t> tempmap;
	UTIL::Image<uint8_t> rainmap;
public:
	Terragen(uint16_t heightres, uint16_t rainres, uint16_t tempres, uint16_t volcres);
	void generate(long seed, const struct MODULE::worldgen_parameters_t *params);
private:
	void gen_heightmap(long seed, const struct MODULE::worldgen_parameters_t *params);
	void gen_tempmap(long seed, const struct MODULE::worldgen_parameters_t *params);
	void gen_rainmap(long seed, const struct MODULE::worldgen_parameters_t *params);
	void gen_volcanism(long seed, const struct MODULE::worldgen_parameters_t *params);
};
