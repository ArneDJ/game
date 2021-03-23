#include <iostream>
#include <vector>
#include <random>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/image.h"
#include "terra.h"

// TODO add these to module config file
#define RAIN_FREQUENCY 0.01F
#define RAIN_OCTAVES 6
#define RAIN_LACUNARITY 3.F
#define RAIN_PERTURB_FREQUENCY 0.01F
#define RAIN_PERTURB_AMP 50.F
#define RAIN_GAUSS_CENTER 0.25F
#define RAIN_GAUSS_SIGMA 0.25F
#define RAIN_DETAIL_MIX 0.5F

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
	gen_tempmap(seed, params->tempfreq, params->tempperturb);

	rainmap->clear();
	gen_rainmap(seed, params->lowland, params->rainblur);
}

void Terragen::gen_heightmap(int64_t seed, const struct worldparams *params)
{
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(params->frequency);
	fastnoise.SetPerturbFrequency(params->perturbfreq);
	fastnoise.SetFractalOctaves(params->octaves);
	fastnoise.SetFractalLacunarity(params->lacunarity);
	fastnoise.SetGradientPerturbAmp(params->perturbamp);

	heightmap->noise(&fastnoise, params->sampling_scale, params->sampling_offset, CHANNEL_RED);
}

void Terragen::gen_tempmap(int64_t seed, float freq, float perturb)
{
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::Perlin);
	fastnoise.SetFrequency(freq);
	fastnoise.SetPerturbFrequency(2.f*freq);
	fastnoise.SetGradientPerturbAmp(perturb);

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

void Terragen::gen_rainmap(int64_t seed, float sealevel, float blur)
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
			uint8_t color = (height > sealevel) ? 255 : 0;
			rainmap->plot(i, j, CHANNEL_RED, color);
		}
	}

	// blur the land mask
	rainmap->blur(blur);

	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::Perlin);
	fastnoise.SetFrequency(RAIN_FREQUENCY);
	fastnoise.SetFractalOctaves(RAIN_OCTAVES);
	fastnoise.SetFractalLacunarity(RAIN_LACUNARITY);
	fastnoise.SetPerturbFrequency(RAIN_PERTURB_FREQUENCY);
	fastnoise.SetGradientPerturbAmp(RAIN_PERTURB_AMP);

	for (int i = 0; i < rainmap->width; i++) {
		for (int j = 0; j < rainmap->height; j++) {
			float rain = 1.f - (rainmap->sample(i, j, CHANNEL_RED) / 255.f);
			float y = i; float x = j;
			fastnoise.GradientPerturbFractal(x, y);
			float detail = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
			float dev = gauss(1.f, RAIN_GAUSS_CENTER, RAIN_GAUSS_SIGMA, rain);
			rain = glm::mix(rain, detail, RAIN_DETAIL_MIX*dev);
			rainmap->plot(i, j, CHANNEL_RED, 255 * glm::clamp(rain, 0.f, 1.f));
		}
	}
}

static inline float gauss(float a, float b, float c, float x)
{
	float exponent = ((x-b)*(x-b)) / (2.f * (c*c));

	return a * std::exp(-exponent);
}
