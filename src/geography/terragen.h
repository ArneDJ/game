
class Terragen {
public:
	util::Image<float> heightmap;
	util::Image<uint8_t> forestation;
	util::Image<uint8_t> tempmap;
	util::Image<uint8_t> rainmap;
public:
	Terragen(uint16_t heightres, uint16_t rainres, uint16_t tempres);
	void generate(long seed, const module::worldgen_parameters_t *params);
private:
	void gen_heightmap(long seed, const module::worldgen_parameters_t *params);
	void gen_tempmap(long seed, const module::worldgen_parameters_t *params);
	void gen_rainmap(long seed, const module::worldgen_parameters_t *params);
	//void gen_volcanism(long seed, const module::worldgen_parameters_t *params);
	void gen_forestation(long seed, const module::worldgen_parameters_t *params);
};
