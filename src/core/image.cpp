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

#include "geom.h"
#include "image.h"

static inline int min3(int a, int b, int c)
{
	return std::min(a, std::min(b, c));
}

static inline int max3(int a, int b, int c)
{
	return std::max(a, std::max(b, c));
}

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
	
void Image::draw_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, uint8_t chan, uint8_t color)
{
	// make sure the triangle is counter clockwise
	if (clockwise(a, b, c)) {
		std::swap(b, c);
	}
	a.x = roundf(a.x);
	a.y = roundf(a.y);
	b.x = roundf(b.x);
	b.y = roundf(b.y);
	c.x = roundf(c.x);
	c.y = roundf(c.y);

	int area = orient(a.x, a.y, b.x, b.y, c.x, c.y);
	if (area == 0) { return; }

	// Compute triangle bounding box
	int minX = min3((int)a.x, (int)b.x, (int)c.x);
	int minY = min3((int)a.y, (int)b.y, (int)c.y);
	int maxX = max3((int)a.x, (int)b.x, (int)c.x);
	int maxY = max3((int)a.y, (int)b.y, (int)c.y);

	// Clip against screen bounds
	minX = std::max(minX, 0);
	minY = std::max(minY, 0);
	maxX = std::min(maxX, width - 1);
	maxY = std::min(maxY, height - 1);

	// Rasterize
	float px, py;
	for (py = minY; py <= maxY; py++) {
		for (px = minX; px <= maxX; px++) {
			// Determine barycentric coordinates
			int w0 = orient(b.x, b.y, c.x, c.y, px, py);
			int w1 = orient(c.x, c.y, a.x, a.y, px, py);
			int w2 = orient(a.x, a.y, b.x, b.y, px, py);

			// If p is on or inside all edges, render pixel.
			if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
				plot(int(px), int(py), chan, color);
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
