#include <chrono>
#include <iostream>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stbimage/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../extern/stbimage/stb_image_write.h"

#include "../extern/fastgaussianblur/fast_gaussian_blur.h"
#include "../extern/fastgaussianblur/fast_gaussian_blur_template.h"

//#include "../extern/fastnoise/FastNoise.h"

#include "image.h"

// normal image creation
Image::Image(uint16_t w, uint16_t h, uint8_t chan)
{
	width = w;
	height = h;
	channels = chan;

	size = width * height * channels;

	data = new uint8_t[size];
	malloced = false;

	clear();
}
	
// load from file
Image::Image(const std::string &filepath)
{
	int w, h, chan;
	data = stbi_load(filepath.c_str(), &w, &h, &chan, 0);
	malloced = true;

	width = w;
	height = h;
	channels = chan;
	size = width * height * channels;
}

Image::~Image(void)
{
	if (data) {
		if (malloced) {
			stbi_image_free(data);
		} else {
			delete [] data;
		}
	}
}
	
void Image::clear(void)
{
	std::memset(data, 0, size);
}
	
void Image::write(const std::string &filepath)
{
	stbi_write_png(filepath.c_str(), width, height, channels, data, channels*width);
}
	
void Image::plot(uint16_t x, uint16_t y, uint8_t chan, uint8_t color)
{
	if (chan >= channels) { return; }

	if (x >= width || y >= height) { return; }

	const size_t index = y * width * channels + x * channels + chan;
	data[index] = color;
}
	
void Image::blur(float sigma)
{
	uint8_t *blurred = new uint8_t[size];
	fast_gaussian_blur_template(data, blurred, width, height, channels, sigma);
	std::memcpy(data, blurred, size);

	delete [] blurred;
}
	
void Image::noise(FastNoise *fastnoise, const glm::vec2 &sample_freq, const glm::vec2 &sample_offset, uint8_t chan)
{
	/*
	FastNoise fastnoise;
	fastnoise.SetSeed(1337);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(0.001f);
	fastnoise.SetPerturbFrequency(0.001f);
	fastnoise.SetFractalOctaves(6);
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetGradientPerturbAmp(200.f);
	*/

	const int nsteps = 32;
	const int stepsize = width / nsteps;

	#pragma omp parallel for
	for (int step_x = 0; step_x < width; step_x += stepsize) {
		int w = step_x + stepsize;
		int h = height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				float x = sample_freq.x * (j + sample_offset.x);
				float y = sample_freq.y * (i + sample_offset.y);
				fastnoise->GradientPerturbFractal(x, y);
				float value = (fastnoise->GetNoise(x, y) + 1.f) / 2.f;
				plot(j, i, chan, 255 * value);
			}
		}
	}
}
