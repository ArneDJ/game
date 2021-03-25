#include <iostream>
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
#include "module.h"
#include "terra.h"
#include "worldgen.h"
#include "atlas.h"

Atlas::Atlas(uint16_t heightres, uint16_t rainres, uint16_t tempres)
{
	terragen = new Terragen { heightres, rainres, tempres };

	struct rectangle area = {
		glm::vec2(0.F, 0.F),
		glm::vec2(scale.x, scale.z)
	};
	worldgen = new Worldgen { area };
}

Atlas::~Atlas(void)
{
	delete terragen;
	delete worldgen;
}

void Atlas::generate(long seed, const struct worldparams *params)
{
	// first generate the world heightmap, rain and temperature data
	terragen->generate(seed, params);

	// then generate the world graph data (mountains, seas, rivers, etc)
	worldgen->generate(seed, params, terragen);
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
		std::copy(data.begin(), data.end(), terragen->heightmap->data);
	} else {
		write_log(LogType::ERROR, "World error: could not load height map");
	}
}

void Atlas::load_rainmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == terragen->rainmap->width && height == terragen->rainmap->height && data.size() == terragen->rainmap->size) {
		std::copy(data.begin(), data.end(), terragen->rainmap->data);
	} else {
		write_log(LogType::ERROR, "World error: could not load rain map");
	}
}

void Atlas::load_tempmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == terragen->tempmap->width && height == terragen->tempmap->height && data.size() == terragen->tempmap->size) {
		std::copy(data.begin(), data.end(), terragen->tempmap->data);
	} else {
		write_log(LogType::ERROR, "World error: could not load temperature map");
	}
}
