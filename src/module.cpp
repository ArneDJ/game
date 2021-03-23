#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "module.h"
	
void Module::init(const std::string &modname)
{
	name = modname;
	path = "modules/" + modname + "/";
	
	std::string worldpath = path + "worldgen.json";

	load_world_parameters(worldpath);
}

void Module::save_world_parameters(void)
{
	const struct worldparams defaultparams = {
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
	params = defaultparams;
	std::string filepath = path + "worldgen.json";

	std::ofstream os(filepath);
	cereal::JSONOutputArchive archive(os);

	archive(cereal::make_nvp("worldgen_config", params));
}

void Module::load_world_parameters(const std::string &filepath)
{
	std::ifstream is(filepath);
	cereal::JSONInputArchive archive(is);

	// TODO error handling
	archive(cereal::make_nvp("worldgen_config", params));
}
