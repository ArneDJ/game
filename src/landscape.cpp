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

static const std::array<FastNoise::CellularReturnType, 5> RIDGE_TYPES = { FastNoise::Distance, FastNoise::Distance2, FastNoise::Distance2Add, FastNoise::Distance2Sub, FastNoise::Distance2Mul };
static const std::array<FastNoise::FractalType, 3> DETAIL_TYPES = { FastNoise::FBM, FastNoise::Billow, FastNoise::RigidMulti };

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

	FastNoise fractalnoise;
	fractalnoise.SetSeed(params.seed);
	fractalnoise.SetNoiseType(FastNoise::SimplexFractal);
	fractalnoise.SetFractalType(params.detail_type);
	fractalnoise.SetFrequency(params.detail_freq);
	fractalnoise.SetPerturbFrequency(params.detail_perturb_freq);
	fractalnoise.SetFractalOctaves(params.detail_octaves);
	fractalnoise.SetFractalLacunarity(params.detail_lacunarity);
	fractalnoise.SetGradientPerturbAmp(params.detail_perturb);

	heightmap->noise(&fractalnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);
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

static struct landgen_parameters random_landgen_parameters(long seed, uint32_t offset)
{
	std::mt19937 gen(seed);
	gen.discard(offset);

	std::uniform_int_distribution<int> noise_seed_distrib;

	std::uniform_real_distribution<float> mountain_freq_distrib(MIN_MOUNTAIN_FREQ, MAX_MOUNTAIN_FREQ);
	std::uniform_real_distribution<float> mountain_perturb_distrib(MIN_MOUNTAIN_PERTURB, MAX_MOUNTAIN_PERTURB);
	std::uniform_int_distribution<uint32_t> mountain_type_distrib(0, RIDGE_TYPES.size()-1);

	std::uniform_int_distribution<uint32_t> detail_type_distrib(0, DETAIL_TYPES.size()-1);
	std::uniform_real_distribution<float> detail_perturb_distrib(MIN_DETAIL_PERTURB, MAX_DETAIL_PERTURB);

	std::uniform_real_distribution<float> sediment_blur_distrib(MIN_SEDIMENT_BLUR, MAX_SEDIMENT_BLUR);

	struct landgen_parameters params;

	params.seed = noise_seed_distrib(gen);

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
