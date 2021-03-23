#pragma once
#include "extern/cereal/types/unordered_map.hpp"
#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"
#include "extern/cereal/archives/json.hpp"

struct heightparams {
	float frequency;
	float perturbfreq;
	float perturbamp;
	uint8_t octaves;
	float lacunarity;
	// heightmap sampling methods
	glm::vec2 sampling_scale;
	glm::vec2 sampling_offset;

	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(
			CEREAL_NVP(frequency), CEREAL_NVP(perturbfreq), 
			CEREAL_NVP(octaves), CEREAL_NVP(lacunarity), 
			cereal::make_nvp("sampling_scale_x", sampling_scale.x), 
			cereal::make_nvp("sampling_scale_y", sampling_scale.y),
			cereal::make_nvp("sampling_offset_x", sampling_offset.x), 
			cereal::make_nvp("sampling_offset_y", sampling_offset.y) 
		);
	}
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

	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(
			CEREAL_NVP(blur), CEREAL_NVP(frequency), 
			CEREAL_NVP(octaves), CEREAL_NVP(lacunarity), 
			CEREAL_NVP(perturb_frequency), CEREAL_NVP(perturb_amp), 
			CEREAL_NVP(gauss_center), CEREAL_NVP(gauss_sigma), 
			CEREAL_NVP(detail_mix) 
		);
	}
};

struct temperatureparams {
	float frequency;
	float perturb;

	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(CEREAL_NVP(frequency), CEREAL_NVP(perturb));
	}
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

	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(
			cereal::make_nvp("heightmap_parameters", height), 
			cereal::make_nvp("temperature_parameters", temperature),
			cereal::make_nvp("rain_parameters", rain), 
			cereal::make_nvp("lowland", lowland),
			cereal::make_nvp("upland", upland), 
			cereal::make_nvp("highland", highland), 
			cereal::make_nvp("erode_mountains", erode_mountains) 
		);
	}
};

class Module {
public:
	// the world generation settings
	struct worldparams params;
public:
	void load(const std::string &modname);
private:
	std::string path;
	std::string name;
private:
	void load_world_parameters(const std::string &filepath);
	// only used when the file is missing
	void save_world_parameters(void);
};
