#include <iostream>
#include <vector>
#include <random>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/image.h"
#include "terra.h"

// The parameter a is the height of the curve's peak, b is the position of the center of the peak and c (the standard deviation, sometimes called the Gaussian RMS width) controls the width of the "bell".
static inline float gauss(float a, float b, float c, float x);

Terragen::Terragen(uint16_t heightres, uint16_t rainres, uint16_t tempres)
{
	heightmap = new FloatImage { heightres, heightres, COLORSPACE_GRAYSCALE };
	rainmap = new Image { rainres, rainres, COLORSPACE_GRAYSCALE };
	tempmap = new Image { tempres, tempres, COLORSPACE_GRAYSCALE };
}

Terragen::~Terragen(void)
{
	delete heightmap;
	delete tempmap;
	delete rainmap;
}
	
void Terragen::generate(int64_t seed, const struct worldparams *params)
{
	heightmap->clear();
	gen_heightmap(seed, params);

	tempmap->clear();
	gen_tempmap(seed, params);

	rainmap->clear();
	gen_rainmap(seed, params);
}

void Terragen::gen_heightmap(int64_t seed, const struct worldparams *params)
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

	heightmap->noise(&fastnoise, params->height.sampling_scale, params->height.sampling_offset, CHANNEL_RED);
}

void Terragen::gen_tempmap(int64_t seed, const struct worldparams *params)
{
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::Perlin);
	fastnoise.SetFrequency(params->temperature.frequency);
	fastnoise.SetPerturbFrequency(2.f*params->temperature.frequency);
	fastnoise.SetGradientPerturbAmp(params->temperature.perturb);

	const float longitude = float(tempmap->height);
	for (int i = 0; i < tempmap->width; i++) {
		for (int j = 0; j < tempmap->height; j++) {
			float y = i; float x = j;
			fastnoise.GradientPerturbFractal(x, y);
			float temperature = 1.f - (y / longitude);
			tempmap->plot(j, i, CHANNEL_RED, 255 * glm::clamp(temperature, 0.f, 1.f));
		}
	}
}

void Terragen::gen_rainmap(int64_t seed, const struct worldparams *params)
{
	// create the land mask image
	// land is white (255), sea is black (0)
	glm::vec2 scale = {
		heightmap->width / rainmap->width,
		heightmap->height / rainmap->height,
	};
	for (int i = 0; i < rainmap->width; i++) {
		for (int j = 0; j < rainmap->height; j++) {
			float height = heightmap->sample(scale.x * i, scale.y * j, CHANNEL_RED);
			uint8_t color = (height > params->lowland) ? 255 : 0;
			rainmap->plot(i, j, CHANNEL_RED, color);
		}
	}

	// blur the land mask
	rainmap->blur(params->rain.blur);

	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::Perlin);
	fastnoise.SetFrequency(params->rain.frequency);
	fastnoise.SetFractalOctaves(params->rain.octaves);
	fastnoise.SetFractalLacunarity(params->rain.lacunarity);
	fastnoise.SetPerturbFrequency(params->rain.perturb_frequency);
	fastnoise.SetGradientPerturbAmp(params->rain.perturb_amp);

	for (int i = 0; i < rainmap->width; i++) {
		for (int j = 0; j < rainmap->height; j++) {
			float rain = 1.f - (rainmap->sample(i, j, CHANNEL_RED) / 255.f);
			float y = i; float x = j;
			fastnoise.GradientPerturbFractal(x, y);
			float detail = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
			float dev = gauss(1.f, params->rain.gauss_center, params->rain.gauss_sigma, rain);
			rain = glm::mix(rain, detail, params->rain.detail_mix*dev);
			rainmap->plot(i, j, CHANNEL_RED, 255 * glm::clamp(rain, 0.f, 1.f));
		}
	}
}

static inline float gauss(float a, float b, float c, float x)
{
	float exponent = ((x-b)*(x-b)) / (2.f * (c*c));

	return a * std::exp(-exponent);
}