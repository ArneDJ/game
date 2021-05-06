#include <iostream>
#include <vector>
#include <random>
#include <queue>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <list>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"

#include "extern/cdt/CDT.h"

#include "extern/poisson/PoissonGenerator.h"

#include "core/logger.h"
#include "core/geom.h"
#include "core/entity.h"
#include "core/image.h"
#include "core/voronoi.h"
#include "module.h"
#include "terragen.h"
#include "worldgraph.h"
#include "mapfield.h"
#include "atlas.h"

enum material_channels {
	CHANNEL_SNOW = 0,
	CHANNEL_GRASS,
	CHANNEL_FARMS,
	CHANNEL_COUNT
};

static const uint8_t SEABED_SMOOTH = 255;
static const uint8_t LOWLAND_SMOOTH = 220;
static const uint8_t UPLAND_SMOOTH = 100;
static const uint8_t HIGHLAND_SMOOTH = 0;
static const float MAP_BLUR_STRENGTH = 5.F;
static const float MAP_SMOOTH_TRANSITION = 5.F;

static const float LAND_DOWNSCALE = 0.8F;

static const uint16_t LANDMAP_RES = 2048;
static const uint16_t WATERMAP_RES = 2048;
static const uint16_t RAINMAP_RES = 512;
static const uint16_t TEMPMAP_RES = 512;
static const uint16_t MATERIALMASKS_RES = 2048;

Atlas::Atlas(void)
{
	terragen = new Terragen { LANDMAP_RES, RAINMAP_RES, TEMPMAP_RES };

	struct rectangle area = {
		glm::vec2(0.F, 0.F),
		glm::vec2(SCALE.x, SCALE.z)
	};
	worldgraph = new Worldgraph { area };

	watermap = new Image { WATERMAP_RES, WATERMAP_RES, COLORSPACE_GRAYSCALE };

	container = new FloatImage { terragen->heightmap->width, terragen->heightmap->height, COLORSPACE_GRAYSCALE };
	detail = new FloatImage { terragen->heightmap->width, terragen->heightmap->height, COLORSPACE_GRAYSCALE };

	mask = new Image { terragen->heightmap->width, terragen->heightmap->height, COLORSPACE_GRAYSCALE };
	
	materialmasks = new Image { MATERIALMASKS_RES, MATERIALMASKS_RES, CHANNEL_COUNT };
	
	vegetation = new Image { RAINMAP_RES, RAINMAP_RES, COLORSPACE_GRAYSCALE };
	tree_density = new Image { RAINMAP_RES, RAINMAP_RES, COLORSPACE_GRAYSCALE };
}

Atlas::~Atlas(void)
{
	clear_entities();

	delete terragen;
	delete worldgraph;

	delete watermap;

	delete container;
	delete detail;
	delete mask;
	
	delete materialmasks;

	delete vegetation;
	delete tree_density;
}

void Atlas::generate(long seedling, const struct worldparams *params)
{
auto start = std::chrono::steady_clock::now();
	holdings.clear();
	holding_tiles.clear();

	mask->clear();
	watermap->clear();

	// first generate the world heightmap, rain and temperature data
	terragen->generate(seedling, params);

	// then generate the world graph data (mountains, seas, rivers, etc)
	worldgraph->generate(seedling, params, terragen);

	// generate holds based on generated world data
	gen_holds();

	smoothe_heightmap();
	plateau_heightmap();
	oregony_heightmap(seedling);

	// now create the watermap
	create_watermap(LAND_DOWNSCALE*params->graph.lowland);
	
	clamp_heightmap(LAND_DOWNSCALE*params->graph.lowland);
	erode_heightmap(0.97f*LAND_DOWNSCALE*params->graph.lowland);

auto end = std::chrono::steady_clock::now();
std::chrono::duration<double> elapsed_seconds = end-start;
std::cout << "campaign image maps time: " << elapsed_seconds.count() << "s\n";
}
	
void Atlas::clear_entities(void)
{
	for (int i = 0; i < trees.size(); i++) {
		delete trees[i];
	}
	trees.clear();

	for (int i = 0; i < settlements.size(); i++) {
		delete settlements[i];
	}
	settlements.clear();
}

void Atlas::smoothe_heightmap(void)
{
	const glm::vec2 mapscale = {
		float(mask->width) / SCALE.x,
		float(mask->height) / SCALE.z
	};

	container->copy(terragen->heightmap);
	container->blur(MAP_BLUR_STRENGTH);

	// create the mask that influences the blur mix
	// higher values means more blur
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		uint8_t color = 0;
		switch (t.relief) {
		case SEABED: color = SEABED_SMOOTH; break;
		case LOWLAND: color = LOWLAND_SMOOTH; break;
		case UPLAND: color = UPLAND_SMOOTH; break;
		case HIGHLAND: color = HIGHLAND_SMOOTH; break;
		}
		if (color > 0) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask->draw_triangle(a, b, c, CHANNEL_RED, color);
			}
		}
	}

	mask->blur(MAP_SMOOTH_TRANSITION);

	// now mix the original map with the blurred version based on the mask
	#pragma omp parallel for
	for (int i = 0; i < terragen->heightmap->width; i++) {
		for (int j = 0; j < terragen->heightmap->height; j++) {
			uint8_t color = mask->sample(i, j, CHANNEL_RED);
			float h = terragen->heightmap->sample(i, j, CHANNEL_RED);
			float b = container->sample(i, j, CHANNEL_RED);
			h = glm::mix(h, b, color / 255.f);
			terragen->heightmap->plot(i, j, CHANNEL_RED, h);
		}
	}

	mask->clear();
}

void Atlas::plateau_heightmap(void)
{
	const glm::vec2 mapscale = {
		float(mask->width) / SCALE.x,
		float(mask->height) / SCALE.z
	};

	// add mountains
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == HIGHLAND) {
			uint8_t color = 255;
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask->draw_triangle(a, b, c, CHANNEL_RED, color);
			}
		}
	}

	// add foothills
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		bool candidate = false;
		if (t.relief == UPLAND) {
			for (const auto &neighbor : t.neighbors) {
				if (neighbor->relief == HIGHLAND) {
					candidate = true;
					break;
				}
			}
		}
		if (candidate) {
			uint8_t color = 255;
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask->draw_triangle(a, b, c, CHANNEL_RED, color);
			}
		}
	}
	
	mask->blur(10.f);

	#pragma omp parallel for
	for (int i = 0; i < terragen->heightmap->width; i++) {
		for (int j = 0; j < terragen->heightmap->height; j++) {
			uint8_t color = mask->sample(i, j, CHANNEL_RED);
			float h = terragen->heightmap->sample(i, j, CHANNEL_RED);
			terragen->heightmap->plot(i, j, CHANNEL_RED, glm::mix(LAND_DOWNSCALE * h, 0.95f * h, color / 255.f));
		}
	}
	
	mask->clear();
}

void Atlas::oregony_heightmap(long seed)
{
	const glm::vec2 mapscale = {
		float(mask->width) / SCALE.x,
		float(mask->height) / SCALE.z
	};

	 // peaks
	FastNoise cellnoise;
	cellnoise.SetSeed(seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(0.06f);
	cellnoise.SetCellularReturnType(FastNoise::Distance2Mul);
	cellnoise.SetGradientPerturbAmp(10.f);
	container->cellnoise(&cellnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);

	FastNoise billow;
	billow.SetSeed(seed);
	billow.SetNoiseType(FastNoise::SimplexFractal);
	billow.SetFractalType(FastNoise::Billow);
	billow.SetFrequency(0.005f);
	billow.SetFractalOctaves(6);
	billow.SetFractalLacunarity(2.5f);
	billow.SetGradientPerturbAmp(40.f);
	detail->noise(&billow, glm::vec2(1.f, 1.f), CHANNEL_RED);

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == HIGHLAND) {
			uint8_t color = 255;
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask->draw_triangle(a, b, c, CHANNEL_RED, color);
			}
		}
	}
	
	mask->blur(3.f);

	#pragma omp parallel for
	for (int i = 0; i < terragen->heightmap->width; i++) {
		for (int j = 0; j < terragen->heightmap->height; j++) {
			uint8_t color = mask->sample(i, j, CHANNEL_RED);
			float height = terragen->heightmap->sample(i, j, CHANNEL_RED);
			float m = container->sample(i, j, CHANNEL_RED);
			float d = detail->sample(i, j, CHANNEL_RED);
			m = 0.3f * glm::mix(d, m, 0.5f);
			height += (color / 255.f) * m;
			terragen->heightmap->plot(i, j, CHANNEL_RED, height);
		}
	}

	mask->clear();
}

void Atlas::erode_heightmap(float ocean_level)
{
	mask->clear();

	const glm::vec2 mapscale = {
		float(mask->width) / SCALE.x,
		float(mask->height) / SCALE.z
	};

	// add the rivers to the mask
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			mask->draw_line(a.x, a.y, b.x, b.y, CHANNEL_RED, 255);
		}
	}

	mask->blur(0.6f);
	// let the rivers erode the land heightmap
	#pragma omp parallel for
	for (int x = 0; x < terragen->heightmap->width; x++) {
		for (int y = 0; y < terragen->heightmap->height; y++) {
			uint8_t masker = mask->sample(x, y, CHANNEL_RED);
			if (masker > 0) {
				float height = terragen->heightmap->sample(x, y, CHANNEL_RED);
				float erosion = glm::clamp(1.f - (masker/255.f), 0.9f, 1.f);
				//float eroded = glm::clamp(0.8f*height, 0.f, 1.f);
				//height = glm::mix(height, eroded, 1.f);
				terragen->heightmap->plot(x, y, CHANNEL_RED, erosion*height);
			}
		}
	}
	
	mask->clear();

	// lower ocean bottom
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == SEABED) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask->draw_triangle(a, b, c, CHANNEL_RED, 255);
			}
		}
	}
	#pragma omp parallel for
	for (int x = 0; x < terragen->heightmap->width; x++) {
		for (int y = 0; y < terragen->heightmap->height; y++) {
			uint8_t masker = mask->sample(x, y, CHANNEL_RED);
			if (masker > 0) {
				float height = terragen->heightmap->sample(x, y, CHANNEL_RED);
				height = glm::clamp(height, 0.f, ocean_level);
				terragen->heightmap->plot(x, y, CHANNEL_RED, height);
			}
		}
	}
}
	
void Atlas::clamp_heightmap(float land_level)
{
	const glm::vec2 mapscale = {
		float(mask->width) / SCALE.x,
		float(mask->height) / SCALE.z
	};

	mask->clear();

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.land) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask->draw_triangle(a, b, c, CHANNEL_RED, 255);
			}
		}
	}

	mask->blur(1.f);

	// ignore rivers
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			mask->draw_thick_line(a.x, a.y, b.x, b.y, 5, CHANNEL_RED, 0);
		}
	}

	#pragma omp parallel for
	for (int x = 0; x < terragen->heightmap->width; x++) {
		for (int y = 0; y < terragen->heightmap->height; y++) {
			uint8_t masker = mask->sample(x, y, CHANNEL_RED);
			if (masker > 0) {
				float height = terragen->heightmap->sample(x, y, CHANNEL_RED);
				height = glm::clamp(height, land_level, 1.f);
				terragen->heightmap->plot(x, y, CHANNEL_RED, height);
			}
		}
	}
}

void Atlas::create_watermap(float ocean_level)
{
auto start = std::chrono::steady_clock::now();
	const glm::vec2 mapscale = {
		float(mask->width) / SCALE.x,
		float(mask->height) / SCALE.z
	};

	mask->clear();

	// create the watermask
	// add the rivers to the mask
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			mask->draw_thick_line(a.x, a.y, b.x, b.y, 5, CHANNEL_RED, 255);
		}
	}

	// now create the heightmap of the water based on the land heightmap
	const glm::vec2 scale = {
		float(terragen->heightmap->width) / float(watermap->width) ,
		float(terragen->heightmap->height) / float(watermap->height)
	};
	for (int x = 0; x < watermap->width; x++) {
		for (int y = 0; y < watermap->height; y++) {
			uint8_t masker = mask->sample(x, y, CHANNEL_RED);
			if (masker > 0) {
				float height = terragen->heightmap->sample(scale.x*x, scale.y*y, CHANNEL_RED);
				height = glm::clamp(height - 0.005f, 0.f, 1.f);
				watermap->plot(x, y, CHANNEL_RED, 255*height);
			}
		}
	}
	

	mask->clear();
	
	// now map the height of the sea tils
	// add the sea tiles to the mask
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.land == true && t.coast == true) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask->draw_triangle(a, b, c, CHANNEL_RED, 255);
			}
		}
	}

	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			mask->draw_thick_line(a.x, a.y, b.x, b.y, 5, CHANNEL_RED, 0);
		}
	}

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == SEABED) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask->draw_triangle(a, b, c, CHANNEL_RED, 255);
			}
		}
	}

	for (int x = 0; x < watermap->width; x++) {
		for (int y = 0; y < watermap->height; y++) {
			uint8_t masker = mask->sample(x, y, CHANNEL_RED);
			if (masker > 0) {
				watermap->plot(x, y, CHANNEL_RED, 255*(ocean_level));
			}
		}
	}
auto end = std::chrono::steady_clock::now();
std::chrono::duration<double> elapsed_seconds = end-start;
std::cout << "elapsed rasterization time: " << elapsed_seconds.count() << "s\n";
}
	
void Atlas::create_materialmasks(void)
{
	materialmasks->clear();

	const glm::vec2 mapscale = {
		float(materialmasks->width) / SCALE.x,
		float(materialmasks->height) / SCALE.z
	};

	// snow
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == HIGHLAND) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				materialmasks->draw_triangle(a, b, c, CHANNEL_SNOW, 255);
			}
		}
	}
	// grass
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == LOWLAND || t.relief == UPLAND) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				materialmasks->draw_triangle(a, b, c, CHANNEL_GRASS, 255);
			}
		}
	}
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.coast || bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			materialmasks->draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_GRASS, 0);
		}
	}

	// farm lands
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.site == CASTLE || t.site == TOWN) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				materialmasks->draw_triangle(a, b, c, CHANNEL_FARMS, 255);
			}
		}
	}
	
	materialmasks->blur(1.f);
}
	
void Atlas::create_vegetation(void)
{
	vegetation->copy(terragen->rainmap);

	// water and mountain tiles don't have vegetation
	const glm::vec2 mapscale = {
		float(vegetation->width) / SCALE.x,
		float(vegetation->height) / SCALE.z
	};

	// snow
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == SEABED || t.relief == HIGHLAND) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				vegetation->draw_triangle(a, b, c, CHANNEL_RED, 0);
			}
		}
	}
	
	// rivers and coasts don't have vegetation
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.coast || bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			vegetation->draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_RED, 0);
		}
	}
}

void Atlas::place_vegetation(long seed)
{
	// spawn the forest
	glm::vec2 hmapscale = {
		float(terragen->heightmap->width) / SCALE.x,
		float(terragen->heightmap->height) / SCALE.z
	};

	std::random_device rd;
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
	std::uniform_real_distribution<float> scale_dist(1.f, 2.f);
	std::uniform_real_distribution<float> density_dist(0.f, 1.f);

	PoissonGenerator::DefaultPRNG PRNG(seed);
	const auto positions = PoissonGenerator::generatePoissonPoints(100000, PRNG, false);

	// density map
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFractalOctaves(2);
	fastnoise.SetFrequency(0.05f);

	tree_density->noise(&fastnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);

	for (const auto &point : positions) {
		float N = tree_density->sample(point.x * tree_density->width, point.y * tree_density->height, CHANNEL_RED) / 255.f;
		if (N < 0.5f) { continue; }

		float P = vegetation->sample(point.x * vegetation->width, point.y * vegetation->height, CHANNEL_RED) / 255.f;

		//P *= rain * rain;
		float R = density_dist(gen);
		if ( R > P ) { continue; }
		glm::vec3 position = { point.x * SCALE.x, 0.f, point.y * SCALE.z };
		position.y = SCALE.y * terragen->heightmap->sample(hmapscale.x*position.x, hmapscale.y*position.z, CHANNEL_RED);
		glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));
		Entity *ent = new Entity { position, rotation };
		ent->scale = 10.f;
		trees.push_back(ent);
	}
}
	
void Atlas::place_settlements(long seed)
{
	std::random_device rd;
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);

	glm::vec2 hmapscale = {
		float(terragen->heightmap->width) / SCALE.x,
		float(terragen->heightmap->height) / SCALE.z
	};

	for (const auto &tile : worldgraph->tiles) {
		if (tile.site == CASTLE || tile.site == TOWN) {
			glm::vec3 position = { tile.center.x, 0.f, tile.center.y };
			position.y = SCALE.y * terragen->heightmap->sample(hmapscale.x*position.x, hmapscale.y*position.z, CHANNEL_RED);
			glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));
			Entity *ent = new Entity { position, rotation };
			settlements.push_back(ent);
		}
	}
}

void Atlas::create_mapdata(long seed)
{
	clear_entities();

	// create the spatial hash field data for the tiles
	gen_mapfield();

	create_materialmasks();

	create_vegetation();
	place_vegetation(seed);

	place_settlements(seed);
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

const Image* Atlas::get_vegetation(void) const
{
	return vegetation;
}
	
const Image* Atlas::get_watermap(void) const
{
	return watermap;
}

const Image* Atlas::get_materialmasks(void) const
{
	return materialmasks;
}
	
const struct navigation_soup* Atlas::get_navsoup(void) const
{
	return &navsoup;
}

const std::vector<Entity*>& Atlas::get_trees(void) const
{
	return trees;
}

const std::vector<Entity*>& Atlas::get_settlements(void) const
{
	return settlements;
}
	
const struct tile* Atlas::tile_at_position(const glm::vec2 &position) const
{
	auto result = mapfield.index_in_field(position);
	
	if (result.found) {
		return &worldgraph->tiles[result.index];
	} else {
		return nullptr;
	}
}
	
void Atlas::load_heightmap(uint16_t width, uint16_t height, const std::vector<float> &data)
{
	if (width == terragen->heightmap->width && height == terragen->heightmap->height && data.size() == terragen->heightmap->size) {
		std::copy(data.begin(), data.end(), terragen->heightmap->data);
	} else {
		write_error_log("World error: could not load height map");
	}
}

void Atlas::load_rainmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == terragen->rainmap->width && height == terragen->rainmap->height && data.size() == terragen->rainmap->size) {
		std::copy(data.begin(), data.end(), terragen->rainmap->data);
	} else {
		write_error_log("World error: could not load rain map");
	}
}

void Atlas::load_tempmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == terragen->tempmap->width && height == terragen->tempmap->height && data.size() == terragen->tempmap->size) {
		std::copy(data.begin(), data.end(), terragen->tempmap->data);
	} else {
		write_error_log("World error: could not load temperature map");
	}
}

void Atlas::load_watermap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == watermap->width && height == watermap->height && data.size() == watermap->size) {
		std::copy(data.begin(), data.end(), watermap->data);
	} else {
		write_error_log("World error: could not load water map");
	}
}

void Atlas::gen_mapfield(void)
{
	std::vector<glm::vec2> vertdata;
	std::vector<struct mosaictriangle> mosaics;
	std::unordered_map<uint32_t, uint32_t> umap;
	uint32_t index = 0;
	for (const auto &c : worldgraph->corners) {
		vertdata.push_back(c.position);
		umap[c.index] = index;
		index++;
	}
	for (const auto &t : worldgraph->tiles) {
		vertdata.push_back(t.center);
		uint32_t a = index;
		index++;
		for (const auto &bord : t.borders) {
			uint32_t b = umap[bord->c0->index];
			uint32_t c = umap[bord->c1->index];
			struct mosaictriangle tri;
			tri.index = t.index;
			tri.a = a;
			tri.b = b;
			tri.c = c;
			mosaics.push_back(tri);
		}
	}

	struct rectangle area = {
		glm::vec2(0.F, 0.F),
		glm::vec2(SCALE.x, SCALE.z)
	};

	mapfield.generate(vertdata, mosaics, area);
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
	
void Atlas::create_land_navigation(void)
{
	navsoup.vertices.clear();
	navsoup.indices.clear();

	struct customedge {
		std::pair<size_t, size_t> vertices;
	};

	using Triangulation = CDT::Triangulation<float>;
	Triangulation cdt = Triangulation(CDT::FindingClosestPoint::ClosestRandom, 10);

	std::vector<glm::vec2> points;
	std::vector<struct customedge> edges;

	// add polygon points for  triangulation
	std::unordered_map<uint32_t, size_t> umap;
	std::unordered_map<uint32_t, bool> marked;
	size_t index = 0;
	for (const auto &c : worldgraph->corners) {
		marked[c.index] = false;
		if (c.coast == true && c.river == false) {
			points.push_back(c.position);
			umap[c.index] = index++;
			marked[c.index] = true;
		} else if (c.frontier == true) {
			bool land = false;
			for (const auto &t : c.touches) {
				if (t->land) { land = true; }
			}
			if (land == true && c.wall == false) {
				points.push_back(c.position);
				umap[c.index] = index++;
				marked[c.index] = true;
			}
		} else if (c.wall == true) {
			points.push_back(c.position);
			umap[c.index] = index++;
			marked[c.index] = true;
		}
	}
	// add special case corners
	for (const auto &c : worldgraph->corners) {
		bool walkable = false;
		if (c.wall) {
			for (const auto &t : c.touches) {
				if (t->relief == LOWLAND || t->relief == HIGHLAND) {
					walkable = true;
				}
			}
		}
		if (marked[c.index] == false && c.frontier == true && c.wall == true && walkable == true) {
			points.push_back(c.position);
			umap[c.index] = index++;
			marked[c.index] = true;
		}
	}

	// make river polygons
	std::map<std::pair<uint32_t, uint32_t>, size_t> tilevertex;
	for (const auto &t : worldgraph->tiles) {
		for (const auto &c : t.corners) {
			if (c->river) {
				glm::vec2 vertex = segment_midpoint(t.center, c->position);
				points.push_back(vertex);
				tilevertex[std::minmax(t.index, c->index)] = index++;
			}
		}
	}
	for (const auto &b : worldgraph->borders) {
		if (b.river) {
			size_t left_t0 = tilevertex[std::minmax(b.t0->index, b.c0->index)];
			size_t right_t0 = tilevertex[std::minmax(b.t0->index, b.c1->index)];
			size_t left_t1 = tilevertex[std::minmax(b.t1->index, b.c0->index)];
			size_t right_t1 = tilevertex[std::minmax(b.t1->index, b.c1->index)];
			struct customedge edge;
			edge.vertices = std::make_pair(left_t0, right_t0);
			edges.push_back(edge);
			edge.vertices = std::make_pair(left_t1, right_t1);
			edges.push_back(edge);
		}
	}

	std::unordered_map<uint32_t, bool> marked_edges;
	std::unordered_map<uint32_t, size_t> edge_vertices;

	// add rivers
	for (const auto &b : worldgraph->borders) {
		bool half_river = b.c0->river ^ b.c1->river;
		marked_edges[b.index] = half_river;
		if (half_river) {
			glm::vec2 vertex = segment_midpoint(b.c0->position, b.c1->position);
			points.push_back(vertex);
			edge_vertices[b.index] = index++;
		} else if (b.river == false && b.c0->river && b.c1->river) {
			glm::vec2 vertex = segment_midpoint(b.c0->position, b.c1->position);
			points.push_back(vertex);
			edge_vertices[b.index] = index++;
			marked_edges[b.index] = true;
		}
	}
	for (const auto &t : worldgraph->tiles) {
		if (t.land) {
			for (const auto &b : t.borders) {
				if (marked_edges[b->index] == true) {
					if (b->c0->river) {
						size_t left = tilevertex[std::minmax(t.index, b->c0->index)];
						size_t right = edge_vertices[b->index];
						struct customedge edge;
						edge.vertices = std::make_pair(left, right);
						edges.push_back(edge);
					}
					if (b->c1->river) {
						size_t left = tilevertex[std::minmax(t.index, b->c1->index)];
						size_t right = edge_vertices[b->index];
						struct customedge edge;
						edge.vertices = std::make_pair(left, right);
						edges.push_back(edge);
					}
				}
			}
		}
	}
	for (const auto &b : worldgraph->borders) {
		if (b.coast) {
			bool half_river = b.c0->river ^ b.c1->river;
			if (half_river) {
				uint32_t index = (b.c0->river == false) ? b.c0->index : b.c1->index; 
				size_t left = umap[index];
				size_t right = edge_vertices[b.index];
				struct customedge edge;
				edge.vertices = std::make_pair(left, right);
				edges.push_back(edge);
			}
		}
	}

	// add coast and mountain edges
	for (const auto &b : worldgraph->borders) {
		size_t left = umap[b.c0->index];
		size_t right = umap[b.c1->index];
		if (marked[b.c0->index] == true && marked[b.c1->index] == true) {
			if (b.coast == true) {
				struct customedge edge;
				edge.vertices = std::make_pair(left, right);
				edges.push_back(edge);
			} else if (b.wall == true) {
				if (b.t0->relief == HIGHLAND ^ b.t1->relief  == HIGHLAND) {
					struct customedge edge;
					edge.vertices = std::make_pair(left, right);
					edges.push_back(edge);
				}
			} else if (b.frontier == true) {
				if (b.t0->land == true && b.t1->land == true) {
					struct customedge edge;
					edge.vertices = std::make_pair(left, right);
					edges.push_back(edge);
				}
			}
		}
	}

	// deluanay triangulation
	cdt.insertVertices(
		points.begin(),
		points.end(),
		[](const glm::vec2& p){ return p[0]; },
		[](const glm::vec2& p){ return p[1]; }
	);
	cdt.insertEdges(
		edges.begin(),
		edges.end(),
		[](const customedge& e){ return e.vertices.first; },
		[](const customedge& e){ return e.vertices.second; }
	);
	cdt.eraseOuterTrianglesAndHoles();

	for (int i = 0; i < cdt.vertices.size(); i++) {
		const auto pos = cdt.vertices[i].pos;
		navsoup.vertices.push_back(pos.x);
		navsoup.vertices.push_back(0.f);
		navsoup.vertices.push_back(pos.y);

	}
	for (const auto &triangle : cdt.triangles) {
		glm::vec2 vertices[3];
		for (int i = 0; i < 3; i++) {
			size_t index = triangle.vertices[i];
			const auto pos = cdt.vertices[index].pos;
			vertices[i].x = pos.x;
			vertices[i].y = pos.y;
		}
		if (clockwise(vertices[0], vertices[1], vertices[2])) {
			navsoup.indices.push_back(triangle.vertices[0]);
			navsoup.indices.push_back(triangle.vertices[1]);
			navsoup.indices.push_back(triangle.vertices[2]);
		} else {
			navsoup.indices.push_back(triangle.vertices[0]);
			navsoup.indices.push_back(triangle.vertices[2]);
			navsoup.indices.push_back(triangle.vertices[1]);
		}
	}
}

void Atlas::create_sea_navigation(void)
{
	navsoup.vertices.clear();
	navsoup.indices.clear();

	struct customedge {
		std::pair<size_t, size_t> vertices;
	};

	using Triangulation = CDT::Triangulation<float>;
	Triangulation cdt = Triangulation(CDT::FindingClosestPoint::ClosestRandom, 10);

	std::vector<glm::vec2> points;
	std::vector<struct customedge> edges;

	// add polygon points for triangulation
	std::unordered_map<uint32_t, size_t> umap;
	std::unordered_map<uint32_t, bool> marked;
	size_t index = 0;
	for (const auto &c : worldgraph->corners) {
		marked[c.index] = false;
		if (c.coast) {
			points.push_back(c.position);
			umap[c.index] = index++;
			marked[c.index] = true;
		} else if (c.frontier == true) {
			bool land = false;
			for (const auto &t : c.touches) {
				if (t->land) { land = true; }
			}
			if (land == false) {
				points.push_back(c.position);
				umap[c.index] = index++;
				marked[c.index] = true;
			}
		}
	}

	// add coast and frontier edges
	for (const auto &b : worldgraph->borders) {
		size_t left = umap[b.c0->index];
		size_t right = umap[b.c1->index];
		if (marked[b.c0->index] == true && marked[b.c1->index] == true) {
			if (b.coast == true) {
				struct customedge edge;
				edge.vertices = std::make_pair(left, right);
				edges.push_back(edge);
			} else if (b.frontier == true) {
				if (b.t0->land == false && b.t1->land == false) {
					struct customedge edge;
					edge.vertices = std::make_pair(left, right);
					edges.push_back(edge);
				}
			}
		}
	}

	// deluanay triangulation
	cdt.insertVertices(
		points.begin(),
		points.end(),
		[](const glm::vec2& p){ return p[0]; },
		[](const glm::vec2& p){ return p[1]; }
	);
	cdt.insertEdges(
		edges.begin(),
		edges.end(),
		[](const customedge& e){ return e.vertices.first; },
		[](const customedge& e){ return e.vertices.second; }
	);
	cdt.eraseOuterTrianglesAndHoles();

	for (int i = 0; i < cdt.vertices.size(); i++) {
		const auto pos = cdt.vertices[i].pos;
		navsoup.vertices.push_back(pos.x);
		navsoup.vertices.push_back(0.f);
		navsoup.vertices.push_back(pos.y);

	}
	for (const auto &triangle : cdt.triangles) {
		glm::vec2 vertices[3];
		for (int i = 0; i < 3; i++) {
			size_t index = triangle.vertices[i];
			const auto pos = cdt.vertices[index].pos;
			vertices[i].x = pos.x;
			vertices[i].y = pos.y;
		}
		if (clockwise(vertices[0], vertices[1], vertices[2])) {
			navsoup.indices.push_back(triangle.vertices[0]);
			navsoup.indices.push_back(triangle.vertices[1]);
			navsoup.indices.push_back(triangle.vertices[2]);
		} else {
			navsoup.indices.push_back(triangle.vertices[0]);
			navsoup.indices.push_back(triangle.vertices[2]);
			navsoup.indices.push_back(triangle.vertices[1]);
		}
	}
}
