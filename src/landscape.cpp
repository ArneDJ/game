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
	fastnoise.SetFrequency(0.001f);
	fastnoise.SetPerturbFrequency(0.001f);
	fastnoise.SetFractalOctaves(6);
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetGradientPerturbAmp(200.f);

	heightmap->noise(&fastnoise, glm::vec2(1.f, 1.f), offset, CHANNEL_RED);
}
