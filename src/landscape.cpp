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

void Landscape::generate(long seed, const glm::vec2 &offset)
{
	gen_heightmap(seed, offset);
}

void Landscape::gen_heightmap(long seed, const glm::vec2 &offset)
{
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	//fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFractalType(FastNoise::Billow);
	//fastnoise.SetFractalType(FastNoise::RigidMulti);
	fastnoise.SetFrequency(0.002f);
	fastnoise.SetPerturbFrequency(0.001f);
	fastnoise.SetFractalOctaves(6); // erosion
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetGradientPerturbAmp(200.f);

	heightmap->noise(&fastnoise, glm::vec2(1.f, 1.f), offset, CHANNEL_RED);

	FloatImage blurred = { heightmap->width, heightmap->height, COLORSPACE_GRAYSCALE };
	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		heightmap->data[i] = height * height;
		blurred.data[i] = height * height;
	}

	blurred.blur(20.f);

	for (int i = 0; i < heightmap->size; i++) {
		float height = heightmap->data[i];
		float blurry = blurred.data[i];
		heightmap->data[i] = glm::mix(blurry, height, height);
	}
}
