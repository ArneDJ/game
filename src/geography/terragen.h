
class Terragen {
public:
	UTIL::FloatImage heightmap;
	UTIL::Image tempmap;
	UTIL::Image rainmap;
public:
	Terragen(uint16_t heightres, uint16_t rainres, uint16_t tempres);
	void generate(long seed, const struct worldparams *params);
private:
	void gen_heightmap(long seed, const struct worldparams *params);
	void gen_tempmap(long seed, const struct worldparams *params);
	void gen_rainmap(long seed, const struct worldparams *params);
};
