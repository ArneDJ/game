#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <list>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/logger.h"
#include "core/geom.h"
#include "core/image.h"
#include "core/voronoi.h"
#include "module.h"
#include "terra.h"
#include "worldgraph.h"
#include "atlas.h"

Atlas::Atlas(uint16_t heightres, uint16_t rainres, uint16_t tempres)
{
	terragen = new Terragen { heightres, rainres, tempres };

	struct rectangle area = {
		glm::vec2(0.F, 0.F),
		glm::vec2(scale.x, scale.z)
	};
	worldgraph = new Worldgraph { area };

	relief = new Image { 2048, 2048, COLORSPACE_GRAYSCALE };
	biomes = new Image { 2048, 2048, COLORSPACE_RGB };
}

Atlas::~Atlas(void)
{
	delete terragen;
	delete worldgraph;
	delete relief;
	delete biomes;
}

void Atlas::generate(long seed, const struct worldparams *params)
{
	// first generate the world heightmap, rain and temperature data
	terragen->generate(seed, params);

	// then generate the world graph data (mountains, seas, rivers, etc)
	worldgraph->generate(seed, params, terragen);

auto start = std::chrono::steady_clock::now();
	// create relief texture
	const glm::vec2 mapscale = {
		float(relief->width) / scale.x,
		float(relief->height) / scale.z
	};

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		uint8_t color = 0;
		switch (t.relief) {
		case SEABED: color = 10; break;
		case LOWLAND: color = 128; break;
		case UPLAND: color = 160; break;
		case HIGHLAND: color = 250; break;
		}
		glm::vec2 a = mapscale * t.center;
		for (const auto &bord : t.borders) {
			glm::vec2 b = mapscale * bord->c0->position;
			glm::vec2 c = mapscale * bord->c1->position;
			relief->draw_triangle(a, b, c, CHANNEL_RED, color);
		}
	}

	glm::vec3 red = {1.f, 0.f, 0.f};
	glm::vec3 sea = {0.2f, 0.5f, 0.95f};
	glm::vec3 grassland = {0.2f, 1.f, 0.2f};
	glm::vec3 desert = {1.f, 1.f, 0.2f};
	glm::vec3 taiga = {0.2f, 0.95f, 0.6f};
	glm::vec3 glacier = {0.8f, 0.8f, 1.f};
	glm::vec3 forest = 0.8f * grassland;
	glm::vec3 taiga_forest = 0.8f * taiga;
	glm::vec3 steppe = glm::mix(grassland, desert, 0.5f);
	glm::vec3 shrubland = glm::mix(forest, desert, 0.75f);
	glm::vec3 savanna = glm::mix(grassland, desert, 0.75f);
	glm::vec3 badlands = glm::mix(red, desert, 0.75f);
	glm::vec3 floodplain = glm::mix(forest, desert, 0.5f);

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		glm::vec3 rgb = {1.f, 1.f, 1.f};
		switch (t.biome) {
		case SEA: rgb = sea; break;
		case BROADLEAF_FOREST: rgb = forest; break;
		case PINE_FOREST: rgb = taiga_forest; break;
		case PINE_GRASSLAND: rgb = taiga; break;
		case SAVANNA: rgb = savanna; break;
		case STEPPE: rgb = steppe; break;
		case DESERT: rgb = desert; break;
		case GLACIER: rgb = glacier; break;
		case SHRUBLAND: rgb = shrubland; break;
		case BROADLEAF_GRASSLAND: rgb = grassland; break;
		case FLOODPLAIN: rgb = floodplain; break;
		case BADLANDS: rgb = badlands; break;
		}
		glm::vec2 a = mapscale * t.center;
		for (const auto &bord : t.borders) {
			glm::vec2 b = mapscale * bord->c0->position;
			glm::vec2 c = mapscale * bord->c1->position;
			biomes->draw_triangle(a, b, c, CHANNEL_RED, 255 * rgb.x);
			biomes->draw_triangle(a, b, c, CHANNEL_GREEN, 255 * rgb.y);
			biomes->draw_triangle(a, b, c, CHANNEL_BLUE, 255 * rgb.z);
		}
	}
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			biomes->draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_RED, 0);
			biomes->draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_GREEN, 0);
			biomes->draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_BLUE, 255);
		}
	}
auto end = std::chrono::steady_clock::now();
std::chrono::duration<double> elapsed_seconds = end-start;
std::cout << "elapsed rasterization time: " << elapsed_seconds.count() << "s\n";
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
	
const Image* Atlas::get_relief(void) const
{
	return relief;
}
	
const Image* Atlas::get_biomes(void) const
{
	return biomes;
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
