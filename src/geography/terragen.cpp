#include <iostream>
#include <memory>
#include <vector>
#include <random>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/cereal/types/unordered_map.hpp"
#include "../extern/cereal/types/vector.hpp"
#include "../extern/cereal/types/memory.hpp"
#include "../extern/cereal/archives/json.hpp"

#include "../util/image.h"
#include "../module.h"
#include "terragen.h"

// The parameter a is the height of the curve's peak, b is the position of the center of the peak and c (the standard deviation, sometimes called the Gaussian RMS width) controls the width of the "bell".
static inline float gauss(float a, float b, float c, float x);

Terragen::Terragen(uint16_t heightres, uint16_t rainres, uint16_t tempres)
{
	heightmap = std::make_unique<UTIL::FloatImage>(heightres, heightres, UTIL::COLORSPACE_GRAYSCALE);
	/*
	rainmap = std::make_unique<UTIL::Image>(rainres, rainres, UTIL::COLORSPACE_GRAYSCALE);
	tempmap = std::make_unique<UTIL::Image>(tempres, tempres, UTIL::COLORSPACE_GRAYSCALE);
	*/
	rainmap.resize(rainres, rainres, UTIL::COLORSPACE_GRAYSCALE);
	tempmap.resize(tempres, tempres, UTIL::COLORSPACE_GRAYSCALE);
}

void Terragen::generate(long seed, const struct worldparams *params)
{
	heightmap->clear();
	gen_heightmap(seed, params);

	tempmap.clear();
	gen_tempmap(seed, params);

	rainmap.clear();
	gen_rainmap(seed, params);
}

void Terragen::gen_heightmap(long seed, const struct worldparams *params)
{
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(params->height.frequency);
	fastnoise.SetPerturbFrequency(params->height.perturbfreq);
	fastnoise.SetFractalOctaves(params->height.octaves);
	fastnoise.SetFractalLacunarity(params->height.lacunarity);
	fastnoise.SetGradientPerturbAmp(params->height.perturbamp);

	heightmap->noise(&fastnoise, params->height.sampling_scale, UTIL::CHANNEL_RED);
}

void Terragen::gen_tempmap(long seed, const struct worldparams *params)
{
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::Perlin);
	fastnoise.SetFrequency(params->temperature.frequency);
	fastnoise.SetPerturbFrequency(2.f*params->temperature.frequency);
	fastnoise.SetGradientPerturbAmp(params->temperature.perturb);

	const float longitude = float(tempmap.height);
	for (int i = 0; i < tempmap.width; i++) {
		for (int j = 0; j < tempmap.height; j++) {
			float y = i; float x = j;
			fastnoise.GradientPerturbFractal(x, y);
			float temperature = 1.f - (y / longitude);
			tempmap.plot(j, i, UTIL::CHANNEL_RED, 255 * glm::clamp(temperature, 0.f, 1.f));
		}
	}
}

void Terragen::gen_rainmap(long seed, const struct worldparams *params)
{
	// create the land mask image
	// land is white (255), sea is black (0)
	glm::vec2 scale = {
		heightmap->width / rainmap.width,
		heightmap->height / rainmap.height,
	};
	for (int i = 0; i < rainmap.width; i++) {
		for (int j = 0; j < rainmap.height; j++) {
			float height = heightmap->sample(scale.x * i, scale.y * j, UTIL::CHANNEL_RED);
			uint8_t color = (height > params->graph.lowland) ? 255 : 0;
			rainmap.plot(i, j, UTIL::CHANNEL_RED, color);
		}
	}

	// blur the land mask
	rainmap.blur(params->rain.blur);

	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::Perlin);
	fastnoise.SetFrequency(params->rain.frequency);
	fastnoise.SetFractalOctaves(params->rain.octaves);
	fastnoise.SetFractalLacunarity(params->rain.lacunarity);
	fastnoise.SetPerturbFrequency(params->rain.perturb_frequency);
	fastnoise.SetGradientPerturbAmp(params->rain.perturb_amp);

	for (int i = 0; i < rainmap.width; i++) {
		for (int j = 0; j < rainmap.height; j++) {
			float rain = 1.f - (rainmap.sample(i, j, UTIL::CHANNEL_RED) / 255.f);
			float y = i; float x = j;
			fastnoise.GradientPerturbFractal(x, y);
			float detail = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
			float dev = gauss(1.f, params->rain.gauss_center, params->rain.gauss_sigma, rain);
			rain = glm::mix(rain, detail, params->rain.detail_mix*dev);
			rain = glm::smoothstep(0.1f, 0.3f, rain);
			rainmap.plot(i, j, UTIL::CHANNEL_RED, 255 * glm::clamp(rain, 0.f, 1.f));
		}
	}
}

static inline float gauss(float a, float b, float c, float x)
{
	float exponent = ((x-b)*(x-b)) / (2.f * (c*c));

	return a * std::exp(-exponent);
}
