#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/image.h"
#include "landscape.h"

static const float SEDIMENT_BLUR = 20.F;

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
	// first we mix two noise maps for the heightmap
	// the first noise is cellular noise to add mountain ridges
	// the second noise is fractal noise to add overall detail to the heightmap
	FastNoise fastnoise;
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	//fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFractalType(FastNoise::Billow);
	//fastnoise.SetFractalType(FastNoise::RigidMulti);
	fastnoise.SetFrequency(2.f*0.001f);
	fastnoise.SetPerturbFrequency(0.001f);
	fastnoise.SetFractalOctaves(6);
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetGradientPerturbAmp(200.f);

	FastNoise cellnoise;
	cellnoise.SetSeed(seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(0.01f);
	cellnoise.SetCellularReturnType(FastNoise::Distance2Add);
	cellnoise.SetGradientPerturbAmp(30.f);

	heightmap->noise(&fastnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);
	container->cellnoise(&cellnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);

	// mix two noise images based on height
	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		height = height * height;
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
	container->blur(SEDIMENT_BLUR);

	for (int i = 0; i < heightmap->size; i++) {
		float height = glm::clamp(heightmap->data[i], 0.f, 1.f);
		float blurry = container->data[i];
		heightmap->data[i] = glm::mix(blurry, height, height);
	}
}
