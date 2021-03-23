
struct heightparams {
	float frequency;
	float perturbfreq;
	float perturbamp;
	uint8_t octaves;
	float lacunarity;
	// heightmap sampling methods
	glm::vec2 sampling_scale;
	glm::vec2 sampling_offset;
};

struct rainparams {
	float blur;
	float frequency;
	uint8_t octaves;
	float lacunarity;
	float perturb_frequency;
	float perturb_amp;
	float gauss_center;
	float gauss_sigma;
	float detail_mix;
};

struct temperatureparams {
	float frequency;
	float perturb;
};

struct worldparams {
	// heightmap
	struct heightparams height;
	// temperatures
	struct temperatureparams temperature;
	// rain
	struct rainparams rain;
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
	void gen_tempmap(int64_t seed, const struct worldparams *params);
	void gen_rainmap(int64_t seed, const struct worldparams *params);
};
