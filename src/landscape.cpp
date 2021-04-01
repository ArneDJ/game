#include <iostream>
#include <vector>
#include <random>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/image.h"
#include "landscape.h"

Landscape::Landscape(uint16_t heightres)
{
	heightmap = new FloatImage { heightres, heightres, COLORSPACE_GRAYSCALE };
}

Landscape::~Landscape(void)
{
	delete heightmap;
}

void Landscape::generate(long seed, float amplitude)
{
	gen_heightmap(seed, amplitude);
}

void Landscape::gen_heightmap(long seed, float amplitude)
{
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::Perlin);
	fastnoise.SetFrequency(5.f*0.001f);
	/*
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	//fastnoise.SetFractalType(FastNoise::FBM);
	//fastnoise.SetFractalType(FastNoise::Billow);
	//fastnoise.SetFractalType(FastNoise::RigidMulti);
	fastnoise.SetFrequency(2.f*0.001f);
	fastnoise.SetPerturbFrequency(0.001f);
	fastnoise.SetFractalOctaves(6); // erosion
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetGradientPerturbAmp(200.f);
	*/

	FloatImage cellular = { heightmap->width, heightmap->height, COLORSPACE_GRAYSCALE };
	FastNoise cellnoise;
	cellnoise.SetSeed(seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(0.01f);
	cellnoise.SetCellularReturnType(FastNoise::Distance2Add);
	cellnoise.SetGradientPerturbAmp(30.f);

	heightmap->noise(&fastnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);
	cellular.cellnoise(&cellnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);

	// mix two noise images
	// apply mask

	// mix with blur based on height
	/*
	FloatImage blurred = { heightmap->width, heightmap->height, COLORSPACE_GRAYSCALE };
	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		height = height * height;
		float cell = cellular.data[i];
		height = glm::mix(height, cell, height);
		heightmap->data[i] = height;
		blurred.data[i] = height;
	}

	blurred.blur(20.f); // sediment

	for (int i = 0; i < heightmap->size; i++) {
		float height = glm::clamp(heightmap->data[i], 0.f, 1.f);
		float blurry = blurred.data[i];
		heightmap->data[i] = amplitude * glm::mix(blurry, height, height);
	}
	*/
}
