#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/aixlog/aixlog.h"

#include "module.h"
	
namespace MODULE {

void Module::load(const std::string &modname)
{
	name = modname;
	path = "modules/" + modname + "/";
	
	load_file(params, path + "worldgen.json");

	load_file(houses, path + "buildings.json");

	// lowland may not be higher than upland
	if (params.graph.lowland > params.graph.upland) {
		std::swap(params.graph.lowland, params.graph.upland);
	}
	// upland may not be higher than highland
	if (params.graph.upland > params.graph.highland) {
		std::swap(params.graph.upland, params.graph.highland);
	}

	load_file(test_armature, path + "ragdolls/human.json");
	load_file(vegetation, path + "vegetation.json");
	load_file(atmosphere, path + "atmosphere.json");
}

template <class T>
void Module::save_file(const T &data, const std::string &name, const std::string &filepath)
{
	std::ofstream stream(filepath);

	if (stream.is_open()) {
		cereal::JSONOutputArchive archive(stream);
		archive(cereal::make_nvp(name, data));
	} else {
		LOG(ERROR, "Module") << "could not save to " + filepath;
	}
}

template <class T>
void Module::load_file(T &data, const std::string &filepath)
{
	std::ifstream stream(filepath);

	if (stream.is_open()) {
		cereal::JSONInputArchive archive(stream);
		archive(data);
	} else {
		LOG(ERROR, "Module") << "could not load " + filepath;
	}
}

};
