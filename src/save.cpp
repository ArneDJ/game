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
#include "save.h"
	
void Saver::save(const std::string &filepath, const Terragen *terra)
{
	topology.width = terra->heightmap->width;
	topology.height = terra->heightmap->height;
	topology.channels = terra->heightmap->channels;
	topology.size = terra->heightmap->size;
	topology.data.resize(terra->heightmap->size);
	// TODO use memcpy
	for (int i = 0; i < terra->heightmap->size; i++) {
		topology.data[i] = terra->heightmap->data[i];
	}

	temperature.width = terra->tempmap->width;
	temperature.height = terra->tempmap->height;
	temperature.channels = terra->tempmap->channels;
	temperature.size = terra->tempmap->size;
	temperature.data.resize(terra->tempmap->size);
	for (int i = 0; i < terra->tempmap->size; i++) {
		temperature.data[i] = terra->tempmap->data[i];
	}

	rain.width = terra->rainmap->width;
	rain.height = terra->rainmap->height;
	rain.channels = terra->rainmap->channels;
	rain.size = terra->rainmap->size;
	rain.data.resize(terra->rainmap->size);
	for (int i = 0; i < terra->rainmap->size; i++) {
		rain.data[i] = terra->rainmap->data[i];
	}

	std::ofstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryOutputArchive archive(stream);
		archive(cereal::make_nvp("topology", topology), cereal::make_nvp("rain", rain), cereal::make_nvp("temperature", temperature));
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + "could not be saved");
	}
}

void Saver::load(const std::string &filepath, Terragen *terra)
{
	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(cereal::make_nvp("topology", topology), cereal::make_nvp("rain", rain), cereal::make_nvp("temperature", temperature));
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + " could not be loaded");
		return;
	}

	if (topology.width == terra->heightmap->width && topology.height == terra->heightmap->height && topology.size == terra->heightmap->size) {
		for (int i = 0; i < terra->heightmap->size; i++) {
			terra->heightmap->data[i] = topology.data[i];
		}
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + " topology could not be loaded");
		return;
	}

	if (rain.width == terra->rainmap->width && rain.height == terra->rainmap->height && rain.size == terra->rainmap->size) {
		for (int i = 0; i < terra->rainmap->size; i++) {
			terra->rainmap->data[i] = rain.data[i];
		}
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + " rain could not be loaded");
		return;
	}

	if (temperature.width == terra->tempmap->width && temperature.height == terra->tempmap->height && temperature.size == terra->tempmap->size) {
		for (int i = 0; i < terra->tempmap->size; i++) {
			terra->tempmap->data[i] = temperature.data[i];
		}
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + " temperature could not be loaded");
		return;
	}
}
