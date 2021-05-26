#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/cereal/types/unordered_map.hpp"
#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"
#include "extern/cereal/archives/json.hpp"
#include "extern/cereal/archives/xml.hpp"

#include "extern/aixlog/aixlog.h"

#include "module.h"
	
void Module::load(const std::string &modname)
{
	name = modname;
	path = "modules/" + modname + "/";
	
	std::string worldpath = path + "worldgen.xml";
	std::string atmospath = path + "colors.xml";
	std::string buildingpath = path + "buildings.xml";

	load_world_parameters(worldpath);

	load_colors(atmospath);

	load_buildings(buildingpath);

	// lowland may not be higher than upland
	if (params.graph.lowland > params.graph.upland) {
		std::swap(params.graph.lowland, params.graph.upland);
	}
	// upland may not be higher than highland
	if (params.graph.upland > params.graph.highland) {
		std::swap(params.graph.upland, params.graph.highland);
	}
}

void Module::save_world_parameters(const std::string &filepath)
{
	std::ofstream stream(filepath);

	if (stream.is_open()) {
		cereal::XMLOutputArchive archive(stream);
		archive(cereal::make_nvp("worldgen_config", params));
	} else {
		LOG(ERROR, "Module") << "could not save to " + filepath;
	}
}

void Module::load_world_parameters(const std::string &filepath)
{
	std::ifstream stream(filepath);

	if (stream.is_open()) {
		cereal::XMLInputArchive archive(stream);
		archive(cereal::make_nvp("worldgen_config", params));
	} else {
		// file not found 
		// use default parameters and save the file
		LOG(ERROR, "Module") << "could not open " + filepath + ", resorting to default parameters";
		params = {};
		save_world_parameters(filepath);
	}
}

void Module::load_colors(const std::string &filepath)
{
	std::ifstream stream(filepath);

	if (stream.is_open()) {
		cereal::XMLInputArchive archive(stream);
		archive(cereal::make_nvp("colors", colors));
	} else {
		// file not found 
		// use default colorization and save the file
		LOG(ERROR, "Module") << "could not open " + filepath + ", resorting to default colors";
		colors = {};
		save_colors(filepath);
	}
}

void Module::save_colors(const std::string &filepath)
{
	std::ofstream stream(filepath);

	if (stream.is_open()) {
		cereal::XMLOutputArchive archive(stream);
		archive(cereal::make_nvp("colors", colors));
	} else {
		LOG(ERROR, "Module") << "could not save to " + filepath;
	}
}

void Module::load_buildings(const std::string &filepath)
{
	std::ifstream stream(filepath);

	if (stream.is_open()) {
		cereal::XMLInputArchive archive(stream);
		archive(cereal::make_nvp("houses", houses));
	} else {
		// file not found 
		LOG(ERROR, "Module") << "could not open " + filepath;
	}
}
