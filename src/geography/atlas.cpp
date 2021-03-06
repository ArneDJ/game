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

#include "../extern/cereal/types/vector.hpp"
#include "../extern/cereal/types/memory.hpp"

#include "../extern/cdt/CDT.h"

#include "../extern/poisson/PoissonGenerator.h"

#include "../extern/aixlog/aixlog.h"

#include "../geometry/geom.h"
#include "../geometry/voronoi.h"
#include "../util/image.h"
#include "../module/module.h"
#include "terragen.h"
#include "worldgraph.h"
#include "mapfield.h"
#include "atlas.h"

namespace geography {

enum material_channels_t {
	CHANNEL_SNOW = 0,
	CHANNEL_GRASS,
	CHANNEL_SAND,
	CHANNEL_ARID,
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
static const uint16_t FACTIONSMAP_RES = 2048;

Atlas::Atlas()
{
	terragen = std::make_unique<Terragen>(LANDMAP_RES, RAINMAP_RES, TEMPMAP_RES);

	geom::rectangle_t area = {
		glm::vec2(0.F, 0.F),
		glm::vec2(SCALE.x, SCALE.z)
	};
	worldgraph = std::make_unique<Worldgraph>(area);

	watermap.resize(WATERMAP_RES, WATERMAP_RES, util::COLORSPACE_GRAYSCALE);

	container.resize(terragen->heightmap.width(), terragen->heightmap.height(), util::COLORSPACE_GRAYSCALE);
	detail.resize(terragen->heightmap.width(), terragen->heightmap.height(), util::COLORSPACE_GRAYSCALE);

	mask.resize(terragen->heightmap.width(), terragen->heightmap.height(), util::COLORSPACE_GRAYSCALE);
	
	materialmasks.resize(MATERIALMASKS_RES, MATERIALMASKS_RES, CHANNEL_COUNT);
	
	vegetation.resize(RAINMAP_RES, RAINMAP_RES, util::COLORSPACE_GRAYSCALE);

	factions.resize(FACTIONSMAP_RES, FACTIONSMAP_RES, util::COLORSPACE_RGB);
}

void Atlas::generate(long seedling, const module::worldgen_parameters_t *params)
{
auto start = std::chrono::steady_clock::now();
	holdings.clear();
	holding_tiles.clear();

	mask.clear();
	watermap.clear();

	// first generate the world heightmap, rain and temperature data
	terragen->generate(seedling, params);

	// then generate the world graph data (mountains, seas, rivers, etc)
	worldgraph->generate(seedling, params, terragen.get());

	// generate holds based on generated world data
	gen_holds();

	smoothe_heightmap();
	plateau_heightmap();
	oregony_heightmap(seedling);

	// prevent land height from being be lower than sea level
	clamp_heightmap(LAND_DOWNSCALE*params->graph.lowland);

	// now create the watermap
	create_watermap(LAND_DOWNSCALE*params->graph.lowland);
	
	erode_heightmap(0.97f*LAND_DOWNSCALE*params->graph.lowland);

auto end = std::chrono::steady_clock::now();
std::chrono::duration<double> elapsed_seconds = end-start;
std::cout << "campaign image maps time: " << elapsed_seconds.count() << "s\n";
}
	
void Atlas::smoothe_heightmap()
{
	const glm::vec2 mapscale = {
		float(mask.width()) / SCALE.x,
		float(mask.height()) / SCALE.z
	};

	container.copy(&terragen->heightmap);
	container.blur(MAP_BLUR_STRENGTH);

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
				mask.draw_triangle(a, b, c, util::CHANNEL_RED, color);
			}
		}
	}

	mask.blur(MAP_SMOOTH_TRANSITION);

	// now mix the original map with the blurred version based on the mask
	#pragma omp parallel for
	for (int i = 0; i < terragen->heightmap.width(); i++) {
		for (int j = 0; j < terragen->heightmap.height(); j++) {
			uint8_t color = mask.sample(i, j, util::CHANNEL_RED);
			float h = terragen->heightmap.sample(i, j, util::CHANNEL_RED);
			float b = container.sample(i, j, util::CHANNEL_RED);
			h = glm::mix(h, b, color / 255.f);
			terragen->heightmap.plot(i, j, util::CHANNEL_RED, h);
		}
	}

	mask.clear();
}

void Atlas::plateau_heightmap()
{
	const glm::vec2 mapscale = {
		float(mask.width()) / SCALE.x,
		float(mask.height()) / SCALE.z
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
				mask.draw_triangle(a, b, c, util::CHANNEL_RED, color);
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
				mask.draw_triangle(a, b, c, util::CHANNEL_RED, color);
			}
		}
	}
	
	mask.blur(10.f);

	#pragma omp parallel for
	for (int i = 0; i < terragen->heightmap.width(); i++) {
		for (int j = 0; j < terragen->heightmap.height(); j++) {
			uint8_t color = mask.sample(i, j, util::CHANNEL_RED);
			float h = terragen->heightmap.sample(i, j, util::CHANNEL_RED);
			terragen->heightmap.plot(i, j, util::CHANNEL_RED, glm::mix(LAND_DOWNSCALE * h, 0.95f * h, color / 255.f));
		}
	}
	
	mask.clear();
}

void Atlas::oregony_heightmap(long seed)
{
	const glm::vec2 mapscale = {
		float(mask.width()) / SCALE.x,
		float(mask.height()) / SCALE.z
	};

	 // peaks
	FastNoise cellnoise;
	cellnoise.SetSeed(seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(0.06f);
	cellnoise.SetCellularReturnType(FastNoise::Distance2Mul);
	cellnoise.SetGradientPerturbAmp(10.f);
	container.cellnoise(&cellnoise, glm::vec2(1.f, 1.f), util::CHANNEL_RED);

	FastNoise billow;
	billow.SetSeed(seed);
	billow.SetNoiseType(FastNoise::SimplexFractal);
	billow.SetFractalType(FastNoise::Billow);
	billow.SetFrequency(0.005f);
	billow.SetFractalOctaves(6);
	billow.SetFractalLacunarity(2.5f);
	billow.SetGradientPerturbAmp(40.f);
	detail.noise(&billow, glm::vec2(1.f, 1.f), util::CHANNEL_RED);

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == HIGHLAND) {
			uint8_t color = 255;
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask.draw_triangle(a, b, c, util::CHANNEL_RED, color);
			}
		}
	}
	
	mask.blur(3.f);

	#pragma omp parallel for
	for (int i = 0; i < terragen->heightmap.width(); i++) {
		for (int j = 0; j < terragen->heightmap.height(); j++) {
			uint8_t color = mask.sample(i, j, util::CHANNEL_RED);
			float height = terragen->heightmap.sample(i, j, util::CHANNEL_RED);
			float m = container.sample(i, j, util::CHANNEL_RED);
			float d = detail.sample(i, j, util::CHANNEL_RED);
			m = 0.3f * glm::mix(d, m, 0.5f);
			height += (color / 255.f) * m;
			terragen->heightmap.plot(i, j, util::CHANNEL_RED, height);
		}
	}

	mask.clear();
}

void Atlas::erode_heightmap(float ocean_level)
{
	mask.clear();

	const glm::vec2 mapscale = {
		float(mask.width()) / SCALE.x,
		float(mask.height()) / SCALE.z
	};

	// add the rivers to the mask
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			mask.draw_line(a.x, a.y, b.x, b.y, util::CHANNEL_RED, 255);
		}
	}

	mask.blur(0.6f);
	// let the rivers erode the land heightmap
	#pragma omp parallel for
	for (int x = 0; x < terragen->heightmap.width(); x++) {
		for (int y = 0; y < terragen->heightmap.height(); y++) {
			uint8_t masker = mask.sample(x, y, util::CHANNEL_RED);
			if (masker > 0) {
				float height = terragen->heightmap.sample(x, y, util::CHANNEL_RED);
				float erosion = glm::clamp(1.f - (masker/255.f), 0.9f, 1.f);
				//float eroded = glm::clamp(0.8f*height, 0.f, 1.f);
				//height = glm::mix(height, eroded, 1.f);
				terragen->heightmap.plot(x, y, util::CHANNEL_RED, erosion*height);
			}
		}
	}
	
	mask.clear();

	// lower ocean bottom
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == SEABED) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask.draw_triangle(a, b, c, util::CHANNEL_RED, 255);
			}
		}
	}

	mask.blur(1.f);

	// add the rivers to the mask
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			mask.draw_line(a.x, a.y, b.x, b.y, util::CHANNEL_RED, 0);
		}
	}

	//mask.write("mask.png");
	#pragma omp parallel for
	for (int x = 0; x < terragen->heightmap.width(); x++) {
		for (int y = 0; y < terragen->heightmap.height(); y++) {
			uint8_t masker = mask.sample(x, y, util::CHANNEL_RED);
			if (masker > 0) {
				float height = terragen->heightmap.sample(x, y, util::CHANNEL_RED);
				float clamped_height = glm::clamp(height, 0.f, ocean_level);
				height = glm::mix(height, clamped_height, masker / 255.f);
				terragen->heightmap.plot(x, y, util::CHANNEL_RED, height);
			}
		}
	}
}
	
void Atlas::clamp_heightmap(float land_level)
{
	const glm::vec2 mapscale = {
		float(mask.width()) / SCALE.x,
		float(mask.height()) / SCALE.z
	};

	mask.clear();

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.land) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask.draw_triangle(a, b, c, util::CHANNEL_RED, 255);
			}
		}
	}

	mask.blur(1.f);

	#pragma omp parallel for
	for (int x = 0; x < terragen->heightmap.width(); x++) {
		for (int y = 0; y < terragen->heightmap.height(); y++) {
			uint8_t masker = mask.sample(x, y, util::CHANNEL_RED);
			if (masker > 0) {
				float height = terragen->heightmap.sample(x, y, util::CHANNEL_RED);
				height = glm::clamp(height, land_level, 1.f);
				terragen->heightmap.plot(x, y, util::CHANNEL_RED, height);
			}
		}
	}
}

void Atlas::create_watermap(float ocean_level)
{
	const glm::vec2 mapscale = {
		float(mask.width()) / SCALE.x,
		float(mask.height()) / SCALE.z
	};

	mask.clear();

	// create the watermask
	// add the rivers to the mask
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			mask.draw_thick_line(a.x, a.y, b.x, b.y, 5, util::CHANNEL_RED, 255);
		}
	}

	// now create the heightmap of the water based on the land heightmap
	const glm::vec2 scale = {
		float(terragen->heightmap.width()) / float(watermap.width()) ,
		float(terragen->heightmap.height()) / float(watermap.height())
	};
	for (int x = 0; x < watermap.width(); x++) {
		for (int y = 0; y < watermap.height(); y++) {
			uint8_t masker = mask.sample(x, y, util::CHANNEL_RED);
			if (masker > 0) {
				float height = terragen->heightmap.sample(scale.x*x, scale.y*y, util::CHANNEL_RED);
				height = glm::clamp(height - 0.005f, 0.f, 1.f);
				watermap.plot(x, y, util::CHANNEL_RED, 255*height);
			}
		}
	}
	

	mask.clear();
	
	// now map the height of the sea tils
	// add the sea tiles to the mask
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.land == true && t.coast == true) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask.draw_triangle(a, b, c, util::CHANNEL_RED, 255);
			}
		}
	}

	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			mask.draw_thick_line(a.x, a.y, b.x, b.y, 5, util::CHANNEL_RED, 0);
		}
	}

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.relief == SEABED) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				mask.draw_triangle(a, b, c, util::CHANNEL_RED, 255);
			}
		}
	}

	for (int x = 0; x < watermap.width(); x++) {
		for (int y = 0; y < watermap.height(); y++) {
			uint8_t masker = mask.sample(x, y, util::CHANNEL_RED);
			if (masker > 0) {
				watermap.plot(x, y, util::CHANNEL_RED, 255*(ocean_level));
			}
		}
	}
}
	
void Atlas::create_materialmasks()
{
	materialmasks.clear();

	const glm::vec2 mapscale = {
		float(materialmasks.width()) / SCALE.x,
		float(materialmasks.height()) / SCALE.z
	};

	// base materials
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		enum material_channels_t channel = CHANNEL_ARID;
		switch (t.regolith) {
		case tile_regolith::SNOW: channel = CHANNEL_SNOW; break;
		case tile_regolith::SAND: channel = CHANNEL_SAND; break;
		case tile_regolith::GRASS: channel = CHANNEL_GRASS; break;
		case tile_regolith::STONE: channel = CHANNEL_ARID; break;
		}
		glm::vec2 a = mapscale * t.center;
		for (const auto &bord : t.borders) {
			glm::vec2 b = mapscale * bord->c0->position;
			glm::vec2 c = mapscale * bord->c1->position;
			materialmasks.draw_triangle(a, b, c, channel, 255);
		}
	}

	// sand near rivers
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			materialmasks.draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_SAND, 255);
			materialmasks.draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_GRASS, 0);
		}
	}

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.coast == true || t.river == true) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				materialmasks.draw_triangle(a, b, c, CHANNEL_SAND, 255);
			}
		}
	}

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.feature == tile_feature::FLOODPLAIN) {
			glm::vec2 a = mapscale * t.center;
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				materialmasks.draw_triangle(a, b, c, CHANNEL_GRASS, 255);
			}
		}
	}

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		if (t.land) {
			if (t.regolith == tile_regolith::SAND || t.regolith == tile_regolith::STONE) {
				glm::vec2 a = mapscale * t.center;
				for (const auto &bord : t.borders) {
					glm::vec2 b = mapscale * bord->c0->position;
					glm::vec2 c = mapscale * bord->c1->position;
					materialmasks.draw_triangle(a, b, c, CHANNEL_ARID, 255);
				}
			}
		}
	}

	materialmasks.blur(5.f);
}
	
void Atlas::create_vegetation(long seed)
{
	//vegetation.copy(&terragen->rainmap);
	vegetation.clear();

	// some tiles like desert or snow don't have vegetation
	const glm::vec2 mapscale = {
		float(vegetation.width()) / SCALE.x,
		float(vegetation.height()) / SCALE.z
	};

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		glm::vec2 a = mapscale * t.center;
		glm::vec2 center = t.center / glm::vec2(SCALE.x, SCALE.z);
		if (t.feature == tile_feature::WOODS) {
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				vegetation.draw_triangle(a, b, c, util::CHANNEL_RED, 255);
			}
		}
	}
	
	vegetation.blur(2.f);
	
	// rivers, coasts and mountain borders don't have vegetation
	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.coast || bord.river || bord.wall) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			vegetation.draw_thick_line(a.x, a.y, b.x, b.y, 1, util::CHANNEL_RED, 0);
		}
	}
	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		glm::vec2 a = mapscale * t.center;
		glm::vec2 center = t.center / glm::vec2(SCALE.x, SCALE.z);
		if (t.regolith != tile_regolith::GRASS) {
			for (const auto &bord : t.borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				vegetation.draw_triangle(a, b, c, util::CHANNEL_RED, 0);
			}
		}
	}
}
	
void Atlas::create_factions_map()
{
	factions.clear();

	const glm::vec2 mapscale = {
		float(factions.width()) / SCALE.x,
		float(factions.height()) / SCALE.z
	};

	// TODO faction colors need to be saved
	std::mt19937 gen(1337);
	std::uniform_real_distribution<float> color_dist(0.f, 1.f);

	for(auto iter = holdings.begin(); iter != holdings.end(); iter++) {
		glm::vec3 color = { color_dist(gen), color_dist(gen), color_dist(gen) };
		colorize_holding(iter->first, color);
	}
}

void Atlas::place_vegetation(long seed)
{
	// spawn the forest
	std::random_device rd;
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
	std::uniform_real_distribution<float> scale_dist(1.f, 2.f);
	std::uniform_real_distribution<float> density_dist(0.f, 1.f);

	PoissonGenerator::DefaultPRNG PRNG(seed);
	const auto positions = PoissonGenerator::generatePoissonPoints(100000, PRNG, false);

	for (const auto &point : positions) {
		uint8_t density = vegetation.sample_scaled(point.x, point.y, util::CHANNEL_RED);
		if (density == 0) { continue; }
		if (density < 255) {
			float rain = density / 255.f;
			float chance = density_dist(gen);
			if (chance > rain) { continue; }
		}
		glm::vec3 position = { point.x * SCALE.x, 0.f, point.y * SCALE.z };
		position.y = SCALE.y * terragen->heightmap.sample_scaled(point.x, point.y, util::CHANNEL_RED);
		glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));
		geom::transformation_t transform = { position, rotation, 10.f };
		trees.push_back(transform);
	}
}
	
void Atlas::create_mapdata(long seed)
{
	trees.clear();

	// create the spatial hash field data for the tiles
	gen_mapfield();

	create_materialmasks();

	create_vegetation(seed);
	place_vegetation(seed);

	holding_tiles.clear();
	for(auto iter = holdings.begin(); iter != holdings.end(); iter++) {
		auto &hold = iter->second;
		for (const auto &tile : hold.lands) {
			holding_tiles[tile] = hold.ID;
		}
	}
	create_factions_map();
}
	
const util::Image<uint8_t>* Atlas::get_vegetation() const
{
	return &vegetation;
}
	
const util::Image<uint8_t>* Atlas::get_watermap() const
{
	return &watermap;
}
	
const Terragen* Atlas::get_terragen() const
{
	return terragen.get();
}

const util::Image<uint8_t>* Atlas::get_materialmasks() const
{
	return &materialmasks;
}

const util::Image<uint8_t>* Atlas::get_factions() const
{
	return &factions;
}
	
const navigation_soup_t& Atlas::get_navsoup() const
{
	return navsoup;
}

const std::unordered_map<uint32_t, holding_t>& Atlas::get_holdings() const
{
	return holdings;
}

const std::unordered_map<uint32_t, uint32_t>& Atlas::get_holding_tiles() const
{
	return holding_tiles;
}
	
const Worldgraph* Atlas::get_worldgraph() const
{
	return worldgraph.get();
}
	
Worldgraph* Atlas::get_worldgraph()
{
	return worldgraph.get();
}

const std::vector<geom::transformation_t>& Atlas::get_trees() const
{
	return trees;
}

const tile_t* Atlas::tile_at_position(const glm::vec2 &position) const
{
	auto result = mapfield.index_in_field(position);
	
	if (result.found) {
		return &worldgraph->tiles[result.index];
	} else {
		return nullptr;
	}
}
	
void Atlas::colorize_holding(uint32_t holding, const glm::vec3 &color)
{
	const glm::vec2 mapscale = {
		float(factions.width()) / SCALE.x,
		float(factions.height()) / SCALE.z
	};

	auto search = holdings.find(holding);
	if (search != holdings.end()) {
		auto &hold = search->second;
		for (auto tileID : hold.lands) {
			const tile_t *t = &worldgraph->tiles[tileID];
			glm::vec2 a = mapscale * t->center;
			for (const auto &bord : t->borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				factions.draw_triangle(a, b, c, util::CHANNEL_RED, 255*color.x);
				factions.draw_triangle(a, b, c, util::CHANNEL_GREEN, 255*color.y);
				factions.draw_triangle(a, b, c, util::CHANNEL_BLUE, 255*color.z);
			}
		}
	}
}
	
void Atlas::gen_mapfield()
{
	std::vector<glm::vec2> vertdata;
	std::vector<mosaictriangle> mosaics;
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
			mosaictriangle tri;
			tri.index = t.index;
			tri.a = a;
			tri.b = b;
			tri.c = c;
			mosaics.push_back(tri);
		}
	}

	geom::rectangle_t area = {
		glm::vec2(0.F, 0.F),
		glm::vec2(SCALE.x, SCALE.z)
	};

	mapfield.generate(vertdata, mosaics, area);
}

void Atlas::gen_holds()
{
	uint32_t index = 0;
	std::vector<tile_t*> candidates;
	std::unordered_map<const tile_t*, bool> visited;
	std::unordered_map<const tile_t*, int> depth;

	// create the holds
	for (auto &t : worldgraph->tiles) {
		visited[&t] = false;
		depth[&t] = 0;
		if (t.feature == tile_feature::SETTLEMENT) {
			candidates.push_back(&t);
			holding_t hold;
			hold.ID = index++;
			hold.center = t.index;
			holdings[hold.ID] = hold;
		}
	}

	// find the nearest hold center for each tile
	for(auto iter = holdings.begin(); iter != holdings.end(); iter++) {
		auto &hold = iter->second;
		holding_tiles[hold.center] = hold.ID;
		std::queue<const tile_t*> queue;
		queue.push(&worldgraph->tiles[hold.center]);
		while (!queue.empty()) {
			const auto node = queue.front();
			queue.pop();
			int layer = depth[node] + 1;
			for (auto border : node->borders) {
				if (border->frontier == false && border->river == false) {
					auto neighbor = border->t0 == node ? border->t1 : border->t0;
					bool valid = neighbor->relief == LOWLAND || neighbor->relief == UPLAND;
					if ((neighbor->feature != tile_feature::SETTLEMENT) && valid == true) {
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
			holdings[ID].lands.push_back(t.index);
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
					holdings[ID0].neighbors.push_back(holdings[ID1].ID);
					holdings[ID1].neighbors.push_back(holdings[ID0].ID);
				}
			}
		}
	}

	// sites always have to be part of a hold
	/*
	for (auto &t : worldgraph->tiles) {
		if (holding_tiles.find(t.index) == holding_tiles.end()) {
			t.site = VACANT;
		}
	}
	*/
}
	
void Atlas::create_land_navigation()
{
	navsoup.vertices.clear();
	navsoup.indices.clear();

	struct custom_edge_t {
		std::pair<size_t, size_t> vertices;
	};

	using Triangulation = CDT::Triangulation<float>;
	Triangulation cdt = Triangulation(CDT::FindingClosestPoint::ClosestRandom, 10);

	std::vector<glm::vec2> points;
	std::vector<custom_edge_t> edges;

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
				glm::vec2 vertex = geom::segment_midpoint(t.center, c->position);
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
			custom_edge_t edge;
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
			glm::vec2 vertex = geom::segment_midpoint(b.c0->position, b.c1->position);
			points.push_back(vertex);
			edge_vertices[b.index] = index++;
		} else if (b.river == false && b.c0->river && b.c1->river) {
			glm::vec2 vertex = geom::segment_midpoint(b.c0->position, b.c1->position);
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
						custom_edge_t edge;
						edge.vertices = std::make_pair(left, right);
						edges.push_back(edge);
					}
					if (b->c1->river) {
						size_t left = tilevertex[std::minmax(t.index, b->c1->index)];
						size_t right = edge_vertices[b->index];
						custom_edge_t edge;
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
				custom_edge_t edge;
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
				custom_edge_t edge;
				edge.vertices = std::make_pair(left, right);
				edges.push_back(edge);
			} else if (b.wall == true) {
				if (b.t0->relief == HIGHLAND ^ b.t1->relief  == HIGHLAND) {
					custom_edge_t edge;
					edge.vertices = std::make_pair(left, right);
					edges.push_back(edge);
				}
			} else if (b.frontier == true) {
				if (b.t0->land == true && b.t1->land == true) {
					custom_edge_t edge;
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
		[](const custom_edge_t& e){ return e.vertices.first; },
		[](const custom_edge_t& e){ return e.vertices.second; }
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
		if (geom::clockwise(vertices[0], vertices[1], vertices[2])) {
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

void Atlas::create_sea_navigation()
{
	navsoup.vertices.clear();
	navsoup.indices.clear();

	struct custom_edge_t {
		std::pair<size_t, size_t> vertices;
	};

	using Triangulation = CDT::Triangulation<float>;
	Triangulation cdt = Triangulation(CDT::FindingClosestPoint::ClosestRandom, 10);

	std::vector<glm::vec2> points;
	std::vector<custom_edge_t> edges;

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
				custom_edge_t edge;
				edge.vertices = std::make_pair(left, right);
				edges.push_back(edge);
			} else if (b.frontier == true) {
				if (b.t0->land == false && b.t1->land == false) {
					custom_edge_t edge;
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
		[](const custom_edge_t& e){ return e.vertices.first; },
		[](const custom_edge_t& e){ return e.vertices.second; }
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
		if (geom::clockwise(vertices[0], vertices[1], vertices[2])) {
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

};
