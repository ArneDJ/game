#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/logger.h"
#include "core/image.h"
#include "terra.h"
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
	// TODO use memcpy
	for (int i = 0; i < heightmap->size; i++) {
		topology.data[i] = heightmap->data[i];
	}

	const Image *tempmap = atlas->get_tempmap();
	temperature.width = tempmap->width;
	temperature.height = tempmap->height;
	temperature.channels = tempmap->channels;
	temperature.size = tempmap->size;
	temperature.data.resize(tempmap->size);
	for (int i = 0; i < tempmap->size; i++) {
		temperature.data[i] = tempmap->data[i];
	}

	const Image *rainmap = atlas->get_rainmap();
	rain.width = rainmap->width;
	rain.height = rainmap->height;
	rain.channels = rainmap->channels;
	rain.size = rainmap->size;
	rain.data.resize(rainmap->size);
	for (int i = 0; i < rainmap->size; i++) {
		rain.data[i] = rainmap->data[i];
	}

	std::ofstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryOutputArchive archive(stream);
		archive(cereal::make_nvp("topology", topology), cereal::make_nvp("rain", rain), cereal::make_nvp("temperature", temperature));
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + "could not be saved");
	}
}

void Saver::load(const std::string &filepath, Atlas *atlas)
{
	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(cereal::make_nvp("topology", topology), cereal::make_nvp("rain", rain), cereal::make_nvp("temperature", temperature));
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + " could not be loaded");
		return;
	}

	atlas->load_heightmap(topology.width, topology.height, topology.data);

	atlas->load_rainmap(rain.width, rain.height, rain.data);

	atlas->load_tempmap(temperature.width, temperature.height, temperature.data);
}
