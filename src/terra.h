
struct worldparams {
	// heightmap
	float frequency;
	float perturbfreq;
	float perturbamp;
	unsigned int octaves;
	float lacunarity;
	// heightmap sampling methods
	glm::vec2 sampling_scale;
	glm::vec2 sampling_offset;
	// temperatures
	float tempfreq;
	float tempperturb;
	// rain
	float rainblur;
	// relief
	float lowland;
	float upland;
	float highland;
	bool erode_mountains;
};

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
	void gen_tempmap(int64_t seed, float freq, float perturb);
	void gen_rainmap(int64_t seed, float sealevel, float blur);
};
