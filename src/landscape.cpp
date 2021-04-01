#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <array>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/image.h"
#include "landscape.h"

struct landgen_parameters {
	int seed;
	float ridge_freq;
	float ridge_perturb;
	FastNoise::CellularReturnType ridge_type;
};

static const float MIN_MOUNTAIN_FREQ = 0.003F;
static const float MAX_MOUNTAIN_FREQ = 0.008F;
static const float MIN_MOUNTAIN_PERTURB = 20.F;
static const float MAX_MOUNTAIN_PERTURB = 50.F;
static const float SEDIMENT_BLUR = 20.F;

static const std::array<FastNoise::CellularReturnType, 5> RIDGE_TYPES = { FastNoise::Distance, FastNoise::Distance2, FastNoise::Distance2Add, FastNoise::Distance2Sub, FastNoise::Distance2Mul };

static struct landgen_parameters random_landgen_parameters(long seed, uint32_t offset);

Landscape::Landscape(uint16_t heightres)
{
	heightmap = new FloatImage { heightres, heightres, COLORSPACE_GRAYSCALE };
	normalmap = new Image { heightres, heightres, COLORSPACE_RGB };

	container = new FloatImage { heightres, heightres, COLORSPACE_GRAYSCALE };
}

Landscape::~Landscape(void)
{
	delete heightmap;
	delete normalmap;
	delete container;
}

void Landscape::generate(long seed, uint32_t offset, float amplitude)
{
	gen_heightmap(seed, offset, amplitude);

	// create the normalmap
	normalmap->create_normalmap(heightmap, 32.f);
}

void Landscape::gen_heightmap(long seed, uint32_t offset, float amplitude)
{
	// random noise config
	struct landgen_parameters params = random_landgen_parameters(seed, offset);

	// first we mix two noise maps for the heightmap
	// the first noise is cellular noise to add mountain ridges
	// the second noise is fractal noise to add overall detail to the heightmap
	FastNoise cellnoise;
	cellnoise.SetSeed(params.seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(params.ridge_freq);
	cellnoise.SetCellularReturnType(params.ridge_type);
	cellnoise.SetGradientPerturbAmp(params.ridge_perturb);

	FastNoise fastnoise;
	fastnoise.SetSeed(params.seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	//fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFractalType(FastNoise::Billow);
	//fastnoise.SetFractalType(FastNoise::RigidMulti);
	fastnoise.SetFrequency(0.001f);
	fastnoise.SetPerturbFrequency(2.f*0.001f);
	fastnoise.SetFractalOctaves(6);
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetGradientPerturbAmp(200.f);

	heightmap->cellnoise(&cellnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);
	/*
	container->cellnoise(&cellnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);

	// mix two noise images based on height
	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		height = height * height;
		float cell = container->data[i];
		height = amplitude * glm::mix(height, cell, height);
		heightmap->data[i] = height;
	}

	*/
	// apply mask
	// apply a mask to lower the amplitude in the center of the map so two armies can fight eachother without having to climb steep cliffs

	// apply blur
	// to simulate erosion and sediment we mix a blurred image with the original heightmap based on the height (lower areas receive more blur, higher areas less)
	// this isn't an accurate erosion model but it is fast and looks decent enough
	/*
	container->copy(heightmap);
	container->blur(SEDIMENT_BLUR);

	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		float blurry = container->data[i];
		height = glm::mix(blurry, height, height);
		height = glm::clamp(height, 0.f, 1.f);
		heightmap->data[i] = height;
	}
	*/
	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		heightmap->data[i] = amplitude * height;
	}
}

static struct landgen_parameters random_landgen_parameters(long seed, uint32_t offset)
{
	std::mt19937 gen(seed);
	gen.discard(offset);
	std::uniform_int_distribution<int> noise_seed_distrib;
	std::uniform_real_distribution<float> mountain_freq_distrib(MIN_MOUNTAIN_FREQ, MAX_MOUNTAIN_FREQ);
	std::uniform_real_distribution<float> mountain_perturb_distrib(MIN_MOUNTAIN_PERTURB, MAX_MOUNTAIN_PERTURB);
	std::uniform_int_distribution<uint32_t> mountain_type_distrib(0, RIDGE_TYPES.size()-1);

	struct landgen_parameters params;

	params.seed = noise_seed_distrib(gen);
	params.ridge_freq = mountain_freq_distrib(gen);
	params.ridge_perturb = mountain_perturb_distrib(gen);
	params.ridge_type = RIDGE_TYPES[mountain_type_distrib(gen)];

	return params;
}
