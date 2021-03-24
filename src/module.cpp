#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/logger.h"
#include "module.h"
	
static const struct worldparams DEFAULT_WORLD_PARAMETERS = {
	{
		0.001f, 0.001f, 200.f, 6, 2.5f,
		glm::vec2(1.f, 1.f), glm::vec2(0.f, 0.f),
	},
	{ 
		0.005f, 100.f, 
	},
	{
		25.f,
		0.01f,
		6,
		3.f,
		0.01f,
		50.f,
		0.25f,
		0.25f,
		0.5f,
	},
	0.48f,
	0.58f,
	0.66f,
	true
};

void Module::load(const std::string &modname)
{
	name = modname;
	path = "modules/" + modname + "/";
	
	std::string worldpath = path + "worldgen.json";

	load_world_parameters(worldpath);
}

void Module::save_world_parameters(const std::string &filepath)
{
	std::ofstream stream(filepath);

	if (stream.is_open()) {
		cereal::JSONOutputArchive archive(stream);
		archive(cereal::make_nvp("worldgen_config", params));
	} else {
		write_log(LogType::ERROR, "Module save error: could not save to " + filepath);
	}
}

void Module::load_world_parameters(const std::string &filepath)
{
	std::ifstream stream(filepath);

	if (stream.is_open()) {
		cereal::JSONInputArchive archive(stream);
		archive(cereal::make_nvp("worldgen_config", params));
	} else {
		// file not found 
		// use default parameters and save the file
		write_log(LogType::ERROR, "Module load error: could not open " + filepath + ", resorting to default parameters");
		params = DEFAULT_WORLD_PARAMETERS;
		save_world_parameters(filepath);
	}
}
