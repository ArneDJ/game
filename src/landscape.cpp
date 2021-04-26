#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <array>
#include <map>
#include <chrono>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/poisson/PoissonGenerator.h"

#include "core/geom.h"
#include "core/entity.h"
#include "core/image.h"
#include "core/texture.h"
#include "core/mesh.h"
#include "core/model.h"
#include "sitegen.h"
#include "landscape.h"

struct landgen_parameters {
	// mountain ridge
	float ridge_freq;
	float ridge_perturb;
	FastNoise::CellularReturnType ridge_type;
	// detail noise
	FastNoise::FractalType detail_type;
	float detail_freq;
	float detail_perturb_freq;
	int detail_octaves;
	float detail_lacunarity;
	float detail_perturb;
	// erosion
	float sediment_blur;
};

static const float MIN_AMPLITUDE = 0.1F;
static const float MAX_AMPLITUDE = 1.F;

static const float MIN_MOUNTAIN_FREQ = 0.003F;
static const float MAX_MOUNTAIN_FREQ = 0.01F;
static const float MIN_MOUNTAIN_PERTURB = 20.F;
static const float MAX_MOUNTAIN_PERTURB = 50.F;
static const float MIN_DETAIL_PERTURB = 150.F;
static const float MAX_DETAIL_PERTURB = 200.F;
static const float DETAIL_FREQ = 0.002F;
static const float DETAIL_PERTURB_FREQ = 0.001F;
static const int DETAIL_OCTAVES = 6;
static const float DETAIL_LACUNARITY = 2.5F;
static const float MIN_SEDIMENT_BLUR = 20.F;
static const float MAX_SEDIMENT_BLUR = 25.F;

static const int MAX_TREES = 40000;
static const uint16_t DENSITY_MAP_RES = 256;

// TODO pass this
static const struct rectangle PLAYABLE_AREA = {
	{ 0.F, 0.F },
	{ 2048.F, 2048.F }
};
static const glm::vec2 OFFSET = {2048.F, 2048.F};

static const std::array<FastNoise::CellularReturnType, 5> RIDGE_TYPES = { FastNoise::Distance, FastNoise::Distance2, FastNoise::Distance2Add, FastNoise::Distance2Sub, FastNoise::Distance2Mul };
static const std::array<FastNoise::FractalType, 3> DETAIL_TYPES = { FastNoise::FBM, FastNoise::Billow, FastNoise::RigidMulti };

static struct landgen_parameters random_landgen_parameters(long seed);

static bool larger_building(const struct building_t &a, const struct building_t &b);

static struct quadrilateral building_box(glm::vec2 center, glm::vec2 halfwidths, float angle);

Landscape::Landscape(uint16_t heightres, const std::vector<const GLTF::Model*> &house_models)
{
	heightmap = new FloatImage { heightres, heightres, COLORSPACE_GRAYSCALE };
	normalmap = new Image { heightres, heightres, COLORSPACE_RGB };

	container = new FloatImage { heightres, heightres, COLORSPACE_GRAYSCALE };
	
	density = new Image { DENSITY_MAP_RES, DENSITY_MAP_RES, COLORSPACE_GRAYSCALE };

	for (const auto &model : house_models) {
		glm::vec3 size = model->bound_max - model->bound_min;
		struct building_t house = {
			model,
			size
		};
		houses.push_back(house);
	}
	// sort house types from largest to smallest
	std::sort(houses.begin(), houses.end(), larger_building);
}

Landscape::~Landscape(void)
{
	clear();

	delete density;
	delete heightmap;
	delete normalmap;
	delete container;
}
	
void Landscape::clear(void)
{
	heightmap->clear();
	normalmap->clear();
	container->clear();

	for (int i = 0; i < trees.size(); i++) {
		delete trees[i];
	}
	trees.clear();

	for (auto &building : houses) {
		for (int i = 0; i < building.entities.size(); i++) {
			delete building.entities[i];
		}
		building.entities.clear();
	}
}

void Landscape::generate(long campaign_seed, uint32_t tileref, int32_t local_seed, float amplitude, uint8_t precipitation, uint8_t site_radius, bool walled)
{
	clear();

	gen_heightmap(campaign_seed, local_seed, amplitude);

	// create the normalmap
	normalmap->create_normalmap(heightmap, 32.f);

	// if scene is a town generate the site
	if (site_radius > 0) {
		sitegen.generate(campaign_seed, tileref, PLAYABLE_AREA, site_radius);
		place_houses(walled);
	}


	gen_forest(local_seed, precipitation);
}

const FloatImage* Landscape::get_heightmap(void) const
{
	return heightmap;
}
	
const std::vector<Entity*>& Landscape::get_trees(void) const
{
	return trees;
}

const std::vector<building_t>& Landscape::get_houses(void) const
{
	return houses;
}

const Image* Landscape::get_normalmap(void) const
{
	return normalmap;
}

void Landscape::gen_forest(int32_t seed, uint8_t precipitation) 
{
	// create density map
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(0.01f);

	density->noise(&fastnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);
	density->write("density.png");

	// spawn the forest
	glm::vec2 hmapscale = {
		float(heightmap->width) / SCALE.x,
		float(heightmap->height) / SCALE.z
	};

	std::random_device rd;
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
	std::uniform_real_distribution<float> scale_dist(1.f, 2.f);
	std::uniform_real_distribution<float> density_dist(0.f, 1.f);

	printf("precipitation %f\n", precipitation / 255.f);

	float rain = precipitation / 255.f;

	PoissonGenerator::DefaultPRNG PRNG(seed);
	const auto positions = PoissonGenerator::generatePoissonPoints(MAX_TREES, PRNG, false);

	for (const auto &point : positions) {
		float P = density->sample(point.x * density->width, point.y * density->height, CHANNEL_RED) / 255.f;
		if (P > 0.8f) { P = 1.f; }
		P *= rain * rain;
		float R = density_dist(gen);
		if ( R > P ) { continue; }
		glm::vec3 position = { point.x * SCALE.x, 0.f, point.y * SCALE.z };
		float slope = 1.f - (normalmap->sample(hmapscale.x*position.x, hmapscale.y*position.z, CHANNEL_GREEN) / 255.f);
		if (slope > 0.15f) {
			continue;
		}
		position.y = sample_heightmap(glm::vec2(position.x,  position.z));
		position.y -= 2.f;
		glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));
		Entity *ent = new Entity { position, rotation };
		ent->scale = 20.f;
		trees.push_back(ent);
	}
}

void Landscape::gen_heightmap(long campaign_seed, int32_t local_seed, float amplitude)
{
	amplitude = glm::clamp(amplitude, MIN_AMPLITUDE, MAX_AMPLITUDE);

	// random noise config
	struct landgen_parameters params = random_landgen_parameters(campaign_seed);

	// first we mix two noise maps for the heightmap
	// the first noise is cellular noise to add mountain ridges
	// the second noise is fractal noise to add overall detail to the heightmap
	FastNoise cellnoise;
	cellnoise.SetSeed(local_seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(params.ridge_freq);
	cellnoise.SetCellularReturnType(params.ridge_type);
	cellnoise.SetGradientPerturbAmp(params.ridge_perturb);

	FastNoise fractalnoise;
	fractalnoise.SetSeed(local_seed);
	fractalnoise.SetNoiseType(FastNoise::SimplexFractal);
	fractalnoise.SetFractalType(params.detail_type);
	fractalnoise.SetFrequency(params.detail_freq);
	fractalnoise.SetPerturbFrequency(params.detail_perturb_freq);
	fractalnoise.SetFractalOctaves(params.detail_octaves);
	fractalnoise.SetFractalLacunarity(params.detail_lacunarity);
	fractalnoise.SetGradientPerturbAmp(params.detail_perturb);

	heightmap->noise(&fractalnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);
	heightmap->normalize(CHANNEL_RED);
	container->cellnoise(&cellnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);

	// mix two noise images based on height
	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		height *= height;
		float cell = container->data[i];
		height = amplitude * glm::mix(height, cell, height);
		heightmap->data[i] = height;
	}

	// apply mask
	// apply a mask to lower the amplitude in the center of the map so two armies can fight eachother without having to climb steep cliffs

	// apply blur
	// to simulate erosion and sediment we mix a blurred image with the original heightmap based on the height (lower areas receive more blur, higher areas less)
	// this isn't an accurate erosion model but it is fast and looks decent enough
	container->copy(heightmap);
	container->blur(params.sediment_blur);

	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		float blurry = container->data[i];
		height = glm::mix(blurry, height, height);
		height = glm::clamp(height, 0.f, 1.f);
		heightmap->data[i] = height;
	}
}
	
void Landscape::place_houses(bool walled)
{
	// create wall cull quads so houses don't go through city walls
	std::vector<struct quadrilateral> wall_boxes;

	if (walled) {
		for (const auto &d : sitegen.districts) {
			for (const auto &sect : d.sections) {
				if (sect->wall) {
					struct segment S = { sect->d0->center, sect->d1->center };
					glm::vec2 mid = segment_midpoint(S.P0, S.P1);
					glm::vec2 direction = glm::normalize(S.P1 - S.P0);
					float angle = atan2(direction.x, direction.y);
					glm::vec2 halfwidths = { 10.f, glm::distance(mid, S.P1) };
					struct quadrilateral box = building_box(mid, halfwidths, angle);
					wall_boxes.push_back(box);
				}
			}
		}
	}

	//printf("n house templates %d\n", houses.size());

	// place the houses
	for (const auto &d : sitegen.districts) {
		for (const auto &parc : d.parcels) {
			float front = glm::distance(parc.quad.b, parc.quad.a);
			float back = glm::distance(parc.quad.c, parc.quad.d);
			float left = glm::distance(parc.quad.b, parc.quad.c);
			float right = glm::distance(parc.quad.a, parc.quad.d);
			float angle = atan2(parc.direction.x, parc.direction.y);
			glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.f, 1.f, 0.f));
			float height = sample_heightmap(parc.centroid+OFFSET);
			glm::vec3 position = { parc.centroid.x+OFFSET.x, height, parc.centroid.y+OFFSET.y };
			for (auto &house : houses) {
				if ((front > house.bounds.x && back > house.bounds.x) && (left > house.bounds.z && right > house.bounds.z)) {
					glm::vec2 halfwidths = {  0.5f*house.bounds.x, 0.5f*house.bounds.z };
					struct quadrilateral box = building_box(parc.centroid, halfwidths, angle);
					bool valid = true;
					if (walled) {
						for (auto &wallbox : wall_boxes) {
							if (quad_quad_intersection(box, wallbox)) {
								valid = false;
								break;
							}
						}
					}
					if (valid) {
						Entity *entity = new Entity { position, rotation };
						house.entities.push_back(entity);
					}

					break;
				}
			}
		}
	}
}

float Landscape::sample_heightmap(const glm::vec2 &real) const
{
	glm::vec2 imagespace = {
		heightmap->width * (real.x / SCALE.x),
		heightmap->height * (real.y / SCALE.z)
	};
	return SCALE.y * heightmap->sample(roundf(imagespace.x), roundf(imagespace.y), CHANNEL_RED);
}
	
static struct landgen_parameters random_landgen_parameters(long seed)
{
	std::mt19937 gen(seed);

	std::uniform_real_distribution<float> mountain_freq_distrib(MIN_MOUNTAIN_FREQ, MAX_MOUNTAIN_FREQ);
	std::uniform_real_distribution<float> mountain_perturb_distrib(MIN_MOUNTAIN_PERTURB, MAX_MOUNTAIN_PERTURB);
	std::uniform_int_distribution<uint32_t> mountain_type_distrib(0, RIDGE_TYPES.size()-1);

	std::uniform_int_distribution<uint32_t> detail_type_distrib(0, DETAIL_TYPES.size()-1);
	std::uniform_real_distribution<float> detail_perturb_distrib(MIN_DETAIL_PERTURB, MAX_DETAIL_PERTURB);

	std::uniform_real_distribution<float> sediment_blur_distrib(MIN_SEDIMENT_BLUR, MAX_SEDIMENT_BLUR);

	struct landgen_parameters params;

	//params.seed = noise_seed_distrib(gen);

	params.ridge_freq = mountain_freq_distrib(gen);
	params.ridge_perturb = mountain_perturb_distrib(gen);
	params.ridge_type = RIDGE_TYPES[mountain_type_distrib(gen)];

	params.detail_type = DETAIL_TYPES[detail_type_distrib(gen)];
	params.detail_freq = DETAIL_FREQ;
	params.detail_perturb_freq = DETAIL_PERTURB_FREQ;
	params.detail_octaves = DETAIL_OCTAVES;
	params.detail_lacunarity = DETAIL_LACUNARITY;
	params.detail_perturb = detail_perturb_distrib(gen);

	params.sediment_blur = sediment_blur_distrib(gen);

	return params;
}

static bool larger_building(const struct building_t &a, const struct building_t &b)
{
	return (a.bounds.x * a.bounds.z) > (b.bounds.x * b.bounds.z);
}

static struct quadrilateral building_box(glm::vec2 center, glm::vec2 halfwidths, float angle)
{
	glm::vec2 a = {-halfwidths.x, halfwidths.y};
	glm::vec2 b = {-halfwidths.x, -halfwidths.y};
	glm::vec2 c = {halfwidths.x, -halfwidths.y};
	glm::vec2 d = {halfwidths.x, halfwidths.y};

	glm::mat2 R = {
		cos(angle), -sin(angle),
		sin(angle), cos(angle)
	};

	struct quadrilateral quad = {
		center + R * a,
		center + R * b,
		center + R * c,
		center + R * d
	};

	return quad;
}
