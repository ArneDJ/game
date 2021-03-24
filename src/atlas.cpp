#include <iostream>
#include <vector>
#include <random>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/logger.h"
#include "core/image.h"
#include "module.h"
#include "terra.h"
#include "atlas.h"

Atlas::Atlas(uint16_t heightres, uint16_t rainres, uint16_t tempres)
{
	terragen = new Terragen { heightres, rainres, tempres };
}

Atlas::~Atlas(void)
{
	delete terragen;
}

void Atlas::generate(int64_t seed, const struct worldparams *params)
{
	terragen->generate(seed, params);
}
	
const FloatImage* Atlas::get_heightmap(void) const
{
	return terragen->heightmap;
}

const Image* Atlas::get_rainmap(void) const
{
	return terragen->rainmap;
}

const Image* Atlas::get_tempmap(void) const
{
	return terragen->tempmap;
}
	
void Atlas::load_heightmap(uint16_t width, uint16_t height, const std::vector<float> &data)
{
	if (width == terragen->heightmap->width && height == terragen->heightmap->height && data.size() == terragen->heightmap->size) {
		//TODO memcpy(
		for (int i = 0; i < data.size(); i++) {
			terragen->heightmap->data[i] = data[i];
		}
	} else {
		write_log(LogType::ERROR, "World error: could not load height map");
	}
}

void Atlas::load_rainmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == terragen->rainmap->width && height == terragen->rainmap->height && data.size() == terragen->rainmap->size) {
		//TODO memcpy(
		for (int i = 0; i < data.size(); i++) {
			terragen->rainmap->data[i] = data[i];
		}
	} else {
		write_log(LogType::ERROR, "World error: could not load rain map");
	}
}

void Atlas::load_tempmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == terragen->tempmap->width && height == terragen->tempmap->height && data.size() == terragen->tempmap->size) {
		//TODO memcpy(
		for (int i = 0; i < data.size(); i++) {
			terragen->tempmap->data[i] = data[i];
		}
	} else {
		write_log(LogType::ERROR, "World error: could not load temperature map");
	}
}
