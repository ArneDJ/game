
struct heightparams {
	float frequency = 0.001f;
	float perturbfreq = 0.001f;
	float perturbamp = 200.f;
	uint8_t octaves = 6;
	float lacunarity = 2.5f;
	// heightmap sampling methods
	glm::vec2 sampling_scale = { 1.f, 1.f };

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(frequency), CEREAL_NVP(perturbfreq), 
			CEREAL_NVP(perturbamp),
			CEREAL_NVP(octaves), CEREAL_NVP(lacunarity), 
			cereal::make_nvp("sampling_scale_x", sampling_scale.x), 
			cereal::make_nvp("sampling_scale_y", sampling_scale.y)
		);
	}
};

struct rainparams {
	float blur = 30.f;
	float frequency = 0.01f;
	uint8_t octaves = 6;
	float lacunarity = 3.f;
	float perturb_frequency = 0.01f;
	float perturb_amp = 50.f;
	float gauss_center = 0.25f;
	float gauss_sigma = 0.25f;
	float detail_mix = 0.5f;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(blur), CEREAL_NVP(frequency), 
			CEREAL_NVP(octaves), CEREAL_NVP(lacunarity), 
			CEREAL_NVP(perturb_frequency), CEREAL_NVP(perturb_amp), 
			CEREAL_NVP(gauss_center), CEREAL_NVP(gauss_sigma), 
			CEREAL_NVP(detail_mix) 
		);
	}
};

struct temperatureparams {
	float frequency = 0.005f;
	float perturb = 100.f;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(frequency), CEREAL_NVP(perturb));
	}
};

struct graphparams {
	float lowland = 0.45f;
	float upland = 0.58f;
	float highland = 0.66f;
	bool erode_mountains = true;
	float poisson_disk_radius = 16.f;
	uint32_t min_water_body = 256;
	uint32_t min_mountain_body = 128;
	uint8_t min_stream_order = 4;
	uint8_t min_branch_size = 3;
	uint8_t min_basin_size = 4;
	uint8_t town_spawn_radius = 8;
	uint8_t castle_spawn_radius = 10;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(lowland), CEREAL_NVP(upland), CEREAL_NVP(highland),
			CEREAL_NVP(erode_mountains), CEREAL_NVP(poisson_disk_radius),
			CEREAL_NVP(min_water_body), CEREAL_NVP(min_mountain_body),
			CEREAL_NVP(min_stream_order),
			CEREAL_NVP(min_branch_size), CEREAL_NVP(min_basin_size),
			CEREAL_NVP(town_spawn_radius), CEREAL_NVP(castle_spawn_radius)
		);
	}
};

struct worldparams {
	// heightmap
	struct heightparams height;
	// temperatures
	struct temperatureparams temperature;
	// rain
	struct rainparams rain;
	// graph data
	struct graphparams graph;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			cereal::make_nvp("heightmap_parameters", height), 
			cereal::make_nvp("temperature_parameters", temperature),
			cereal::make_nvp("rain_parameters", rain), 
			cereal::make_nvp("graph_parameters", graph)
		);
	}
};

struct atmosphere {
	glm::vec3 ambient = { 1.f, 1.f, 1.f };
	glm::vec3 skytop = { 0.447f, 0.639f, 0.784f };
	glm::vec3 skybottom = { 0.647f, 0.623f, 0.672f };
	
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			cereal::make_nvp("ambient_x", ambient.x), cereal::make_nvp("ambient_y", ambient.y), cereal::make_nvp("ambient_z", ambient.z),
			cereal::make_nvp("skytop_x", skytop.x), cereal::make_nvp("skytop_y", skytop.y), cereal::make_nvp("skytop_z", skytop.z),
			cereal::make_nvp("skybottom_x", skybottom.x), cereal::make_nvp("skybottom_y", skybottom.y), cereal::make_nvp("skybottom_z", skybottom.z)
		);
	}
};

class Module {
public:
	// the world generation settings
	struct worldparams params;
	struct atmosphere atmos;
	std::string path;
	std::string name;
public:
	void load(const std::string &modname);
private:
	void load_world_parameters(const std::string &filepath);
	void load_atmosphere(const std::string &filepath);
	// only used when the file is missing
	void save_world_parameters(const std::string &filepath);
	void save_atmosphere(const std::string &filepath);
};
