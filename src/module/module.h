#pragma once
#include "../util/serialize.h"

namespace module {

struct height_parameters_t {
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

struct rain_parameters_t {
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

struct temperature_parameters_t {
	float frequency = 0.005f;
	float perturb = 100.f;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(frequency), CEREAL_NVP(perturb));
	}
};

struct graph_parameters_t {
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

struct worldgen_parameters_t {
	// heightmap
	struct height_parameters_t height;
	// temperatures
	struct temperature_parameters_t temperature;
	// rain
	struct rain_parameters_t rain;
	// graph data
	struct graph_parameters_t graph;

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

template <class T> struct bounds_t {
	T min;
	T max;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(min), CEREAL_NVP(max));
	}
};

struct weather_t {
	glm::vec3 ambient = { 1.f, 1.f, 1.f };
	glm::vec3 zenith = { 0.525f, 0.735f, 0.84f };
	glm::vec3 horizon = { 0.725f, 0.735f, 0.74f };

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(ambient), CEREAL_NVP(zenith), CEREAL_NVP(horizon));
	}
};

struct atmosphere_t {
	struct weather_t dawn;
	struct weather_t day;
	struct weather_t dusk;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(dawn), CEREAL_NVP(day), CEREAL_NVP(dusk));
	}
};

struct tree_t {
	std::string trunk;
	std::string leaves;
	std::string billboard;
	struct bounds_t<uint8_t> precipitation;
	struct bounds_t<uint8_t> temperature;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(trunk),
			CEREAL_NVP(leaves),
			CEREAL_NVP(billboard),
			CEREAL_NVP(precipitation),
			CEREAL_NVP(temperature)
		);
	}
};

struct grass_t {
	std::string model;
	struct bounds_t<uint8_t> precipitation;
	struct bounds_t<uint8_t> temperature;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(model),
			CEREAL_NVP(precipitation),
			CEREAL_NVP(temperature)
		);
	}
};

struct vegetation_t {
	std::vector<tree_t> trees;
	std::vector<grass_t> grasses;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(trees),
			CEREAL_NVP(grasses)
		);
	}
};

struct palette_t {
	bounds_t<glm::vec3> grass;
	bounds_t<glm::vec3> rock_base;
	bounds_t<glm::vec3> rock_desert;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(grass), 
			CEREAL_NVP(rock_base),
			CEREAL_NVP(rock_desert)
		);
	}
};

struct building_t {
	std::string model;
	struct bounds_t<uint8_t> precipitation;
	struct bounds_t<uint8_t> temperature;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(model),
			CEREAL_NVP(precipitation),
			CEREAL_NVP(temperature)
		);
	}
};

struct ragdoll_bone_import_t {
	std::vector<std::string> targets;
	float radius = 1.f;
	float height = 1.f;
	glm::vec3 origin;
	glm::vec3 rotation; // euler angles

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			cereal::make_nvp("targets", targets), 
			cereal::make_nvp("radius", radius),
			cereal::make_nvp("height", height),
			cereal::make_nvp("origin", origin),
			cereal::make_nvp("rotation", rotation)
		);
	}
};

struct ragdoll_constraint_import_t {
	uint8_t bone;
	glm::vec3 origin;
	glm::vec3 rotation;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			cereal::make_nvp("bone", bone), 
			cereal::make_nvp("origin", origin),
			cereal::make_nvp("rotation", rotation)
		);
	}
};

struct ragdoll_joint_import_t {
	std::string type;
	glm::vec3 limit;
	struct ragdoll_constraint_import_t parent;
	struct ragdoll_constraint_import_t child;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			cereal::make_nvp("type", type), 
			cereal::make_nvp("limit", limit),
			cereal::make_nvp("parent", parent),
			cereal::make_nvp("child", child)
		);
	}
};

struct ragdoll_armature_import_t {
	std::vector<struct ragdoll_bone_import_t> bones;
	std::vector<struct ragdoll_joint_import_t> joints;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			cereal::make_nvp("bones", bones), 
			cereal::make_nvp("joints", joints)
		);
	}
};

class Module {
public:
	// the world generation settings
	worldgen_parameters_t params;
	ragdoll_armature_import_t test_armature;
	std::string path;
	std::string name;
public:
	vegetation_t vegetation;
	atmosphere_t atmosphere;
	palette_t palette;
	std::vector<building_t> houses;
public:
	void load(const std::string &modname);
private:
	template <class T>
	void save_file(const T &data, const std::string &name, const std::string &filepath);
	template <class T>
	void load_file(T &data, const std::string &filepath);
};

};

