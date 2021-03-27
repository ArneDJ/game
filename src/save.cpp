#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <list>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/logger.h"
#include "core/geom.h"
#include "core/image.h"
#include "core/voronoi.h"
#include "terra.h"
#include "worldgraph.h"
#include "atlas.h"
#include "save.h"
	
void Saver::save(const std::string &filepath, const Atlas *atlas)
{
	const FloatImage *heightmap = atlas->get_heightmap();
	topology.width = heightmap->width;
	topology.height = heightmap->height;
	topology.channels = heightmap->channels;
	topology.size = heightmap->size;
	topology.data.resize(heightmap->size);

	std::copy(heightmap->data, heightmap->data + heightmap->size, topology.data.begin());

	const Image *tempmap = atlas->get_tempmap();
	temperature.width = tempmap->width;
	temperature.height = tempmap->height;
	temperature.channels = tempmap->channels;
	temperature.size = tempmap->size;
	temperature.data.resize(tempmap->size);

	std::copy(tempmap->data, tempmap->data + tempmap->size, temperature.data.begin());

	const Image *rainmap = atlas->get_rainmap();
	rain.width = rainmap->width;
	rain.height = rainmap->height;
	rain.channels = rainmap->channels;
	rain.size = rainmap->size;
	rain.data.resize(rainmap->size);

	std::copy(rainmap->data, rainmap->data + rainmap->size, rain.data.begin());

	const Worldgraph *worldgraph = atlas->worldgraph;

	std::ofstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryOutputArchive archive(stream);
		archive(
			cereal::make_nvp("topology", topology), 
			cereal::make_nvp("rain", rain), 
			cereal::make_nvp("temperature", temperature),
			cereal::make_nvp("seed", atlas->seed),
			cereal::make_nvp("tiles", worldgraph->tiles),
			cereal::make_nvp("corners", worldgraph->corners),
			cereal::make_nvp("borders", worldgraph->borders)
		);
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + "could not be saved");
	}
}

void Saver::load(const std::string &filepath, Atlas *atlas)
{
	long seed;

	Worldgraph *worldgraph = atlas->worldgraph;

	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(
			cereal::make_nvp("topology", topology), 
			cereal::make_nvp("rain", rain), 
			cereal::make_nvp("temperature", temperature),
			cereal::make_nvp("seed", seed),
			cereal::make_nvp("tiles", worldgraph->tiles),
			cereal::make_nvp("corners", worldgraph->corners),
			cereal::make_nvp("borders", worldgraph->borders)
		);
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + " could not be loaded");
		return;
	}

	atlas->seed = seed;

	atlas->load_heightmap(topology.width, topology.height, topology.data);

	atlas->load_rainmap(rain.width, rain.height, rain.data);

	atlas->load_tempmap(temperature.width, temperature.height, temperature.data);

	worldgraph->reload_references();
}
