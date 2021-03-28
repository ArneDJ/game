#include <iostream>
#include <vector>
#include <random>
#include <queue>
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

void Atlas::generate(long seedling, const struct worldparams *params)
{
	holdings.clear();
	holding_tiles.clear();

	seed = seedling;
	// first generate the world heightmap, rain and temperature data
	terragen->generate(seed, params);

	// then generate the world graph data (mountains, seas, rivers, etc)
	worldgraph->generate(seed, params, terragen);

	// generate holds based on generated world data
	gen_holds();
}
	
void Atlas::create_maps(void)
{
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

	std::random_device rd;
	std::mt19937 gen(seed);
	#pragma omp parallel for
	for (const auto &hold : holdings) {
		glm::vec3 rgb = {1.f, 1.f, 1.f};
		std::uniform_real_distribution<float> distrib(0.f, 1.f);
		rgb.x = distrib(gen);
		rgb.y = distrib(gen);
		rgb.z = distrib(gen);
		for (const auto &land : hold.lands) {
			glm::vec2 a = mapscale * land->center;
			for (const auto &bord : land->borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				biomes->draw_triangle(a, b, c, CHANNEL_RED, 255 * rgb.x);
				biomes->draw_triangle(a, b, c, CHANNEL_GREEN, 255 * rgb.y);
				biomes->draw_triangle(a, b, c, CHANNEL_BLUE, 255 * rgb.z);
			}
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

void Atlas::gen_holds(void)
{
	uint32_t index = 0;
	std::vector<struct tile*> candidates;
	std::unordered_map<const struct tile*, bool> visited;
	std::unordered_map<const struct tile*, int> depth;

	// create the holds
	for (auto &t : worldgraph->tiles) {
		visited[&t] = false;
		depth[&t] = 0;
		if (t.site == TOWN || t.site == CASTLE) {
			candidates.push_back(&t);
			struct holding hold;
			hold.ID = index++;
			hold.name = "unnamed";
			hold.center = &t;
			holdings.push_back(hold);
		}
	}

	// find the nearest hold center for each tile
	for (auto &hold : holdings) {
		holding_tiles[hold.center->index] = hold.ID;
		std::queue<const struct tile*> queue;
		queue.push(hold.center);
		while (!queue.empty()) {
			const struct tile *node = queue.front();
			queue.pop();
			int layer = depth[node] + 1;
			for (auto border : node->borders) {
				if (border->frontier == false && border->river == false) {
					struct tile *neighbor = border->t0 == node ? border->t1 : border->t0;
					bool valid = neighbor->relief == LOWLAND || neighbor->relief == UPLAND;
					if ((neighbor->site == VACANT || neighbor->site == RESOURCE) && valid == true) {
						if (visited[neighbor] == false) {
							visited[neighbor] = true;
							depth[neighbor] = layer;
							queue.push(neighbor);
							holding_tiles[neighbor->index] = hold.ID;
						} else if (depth[neighbor] > layer) {
							depth[neighbor] = layer;
							queue.push(neighbor);
							holding_tiles[neighbor->index] = hold.ID;
						}
					}
				}
			}
		}
	}

	// add tiles to holding
	for (auto &t : worldgraph->tiles) {
		if (holding_tiles.find(t.index) != holding_tiles.end()) {
			uint32_t ID = holding_tiles[t.index];
			holdings[ID].lands.push_back(&t);
		}
	}

	// find neighbor holdings
	std::map<std::pair<uint32_t, uint32_t>, bool> link;
	for (auto &bord : worldgraph->borders) {
		if (holding_tiles.find(bord.t0->index) != holding_tiles.end() && holding_tiles.find(bord.t1->index) != holding_tiles.end()) {
			uint32_t ID0 = holding_tiles[bord.t0->index];
			uint32_t ID1 = holding_tiles[bord.t1->index];
			if (ID0 != ID1) {
				if (link[std::minmax(ID0, ID1)] == false) {
					link[std::minmax(ID0, ID1)] = true;
					holdings[ID0].neighbors.push_back(&holdings[ID1]);
					holdings[ID1].neighbors.push_back(&holdings[ID0]);
				}
			}
		}
	}

	// sites always have to be part of a hold
	for (auto &t : worldgraph->tiles) {
		if (holding_tiles.find(t.index) == holding_tiles.end()) {
			t.site = VACANT;
		}
	}
}
