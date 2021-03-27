#pragma once
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/archives/json.hpp"

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
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(frequency), CEREAL_NVP(perturbfreq), 
			CEREAL_NVP(perturbamp),
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
	float frequency;
	float perturb;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(frequency), CEREAL_NVP(perturb));
	}
};

struct graphparams {
	float lowland;
	float upland;
	float highland;
	bool erode_mountains;
	float poisson_disk_radius;
	uint32_t min_water_body;
	uint32_t min_mountain_body;
	uint8_t min_stream_order;
	uint8_t min_branch_size;
	uint8_t min_basin_size;
	uint8_t town_spawn_radius;
	uint8_t castle_spawn_radius;

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
	glm::vec3 ambient;
	glm::vec3 skytop;
	glm::vec3 skybottom;

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
public:
	void load(const std::string &modname);
private:
	std::string path;
	std::string name;
private:
	void load_world_parameters(const std::string &filepath);
	void load_atmosphere(const std::string &filepath);
	// only used when the file is missing
	void save_world_parameters(const std::string &filepath);
	void save_atmosphere(const std::string &filepath);
};
