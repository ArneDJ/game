#include <chrono>
#include <iostream>
#include <cstring>
#include <vector>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stbimage/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../extern/stbimage/stb_image_write.h"

#include "../extern/fastgaussianblur/fast_gaussian_blur.h"
#include "../extern/fastgaussianblur/fast_gaussian_blur_template.h"

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
	
void Image::write(const std::string &filepath) const
{
	stbi_write_png(filepath.c_str(), width, height, channels, data, channels*width);
}

uint8_t Image::sample(uint16_t x, uint16_t y, uint8_t chan) const
{
	if (chan >= channels) { return 0; }

	if (x >= width || y >= height) { return 0; }

	const size_t index = y * width * channels + x * channels + chan;

	return data[index];
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
				float value = 0.5f * (fastnoise->GetNoise(x, y) + 1.f);
				plot(j, i, chan, 255 * glm::clamp(value, 0.f, 1.f));
			}
		}
	}
}

FloatImage::FloatImage(uint16_t w, uint16_t h, uint8_t chan)
{
	width = w;
	height = h;
	channels = chan;

	size = width * height * channels;
	data = new float[size];

	clear();
}

FloatImage::~FloatImage(void)
{
	if (data) {
		delete [] data;
	}
}

float FloatImage::sample(uint16_t x, uint16_t y, uint8_t chan) const
{
	if (chan >= channels) { return 0.f; }

	if (x >= width || y >= height) { return 0.f; }

	const size_t index = y * width * channels + x * channels + chan;

	return data[index];
}

void FloatImage::plot(uint16_t x, uint16_t y, uint8_t chan, float color)
{
	if (chan >= channels) { return; }

	if (x >= width || y >= height) { return; }

	const size_t index = y * width * channels + x * channels + chan;
	data[index] = color;
}

void FloatImage::blur(float sigma)
{
	float *blurred = new float[size];
	fast_gaussian_blur_template(data, blurred, width, height, channels, sigma);
	std::memcpy(data, blurred, size * sizeof(float));

	delete [] blurred;
}

void FloatImage::clear(void)
{
	std::memset(data, 0, size * sizeof(float));
}

void FloatImage::noise(FastNoise *fastnoise, const glm::vec2 &sample_freq, const glm::vec2 &sample_offset, uint8_t chan)
{
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
				float value = 0.5f * (fastnoise->GetNoise(x, y) + 1.f);
				plot(j, i, chan, glm::clamp(value, 0.f, 1.f));
			}
		}
	}
}
