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

// http://members.chello.at/~easyfilter/bresenham.html
void Image::draw_line(int x0, int y0, int x1, int y1, uint8_t chan, uint8_t color)
{
	int dx =  abs(x1-x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1-y0), sy = y0 < y1 ? 1 : -1;
	int err = dx+dy, e2; // error value e_xy

	for (;;) {
		plot(x0, y0, chan, color);
		if (x0 == x1 && y0 == y1) { break; }
		e2 = 2*err;
		if (e2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0
		if (e2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0
	}
}

void Image::draw_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, uint8_t chan, uint8_t color)
{
	// make sure the triangle is counter clockwise
	if (clockwise(a, b, c)) {
		std::swap(b, c);
	}

	draw_line(a.x, a.y, b.x, b.y, chan, color);
	draw_line(b.x, b.y, c.x, c.y, chan, color);
	draw_line(c.x, c.y, a.x, a.y, chan, color);

	a.x = floorf(a.x);
	a.y = floorf(a.y);
	b.x = floorf(b.x);
	b.y = floorf(b.y);
	c.x = floorf(c.x);
	c.y = floorf(c.y);

	// Compute triangle bounding box
	int minX = min3(int(a.x), int(b.x), int(c.x));
	int minY = min3(int(a.y), int(b.y), int(c.y));
	int maxX = max3(int(a.x), int(b.x), int(c.x));
	int maxY = max3(int(a.y), int(b.y), int(c.y));

	// Clip against screen bounds
	minX = std::max(minX, 0);
	minY = std::max(minY, 0);
	maxX = std::min(maxX, width - 1);
	maxY = std::min(maxY, height - 1);

	// Triangle setup
	int A01 = a.y - b.y, B01 = b.x - a.x;
	int A12 = b.y - c.y, B12 = c.x - b.x;
	int A20 = c.y - a.y, B20 = a.x - c.x;

	// Barycentric coordinates at minX/minY corner
	glm::ivec2 p = { minX, minY };
	// TODO orient2D
	int w0_row = orient(b.x, b.y, c.x, c.y, p.x, p.y);
	int w1_row = orient(c.x, c.y, a.x, a.y, p.x, p.y);
	int w2_row = orient(a.x, a.y, b.x, b.y, p.x, p.y);

	// Rasterize
	for (p.y = minY; p.y <= maxY; p.y++) {
		// Barycentric coordinates at start of row
		int w0 = w0_row;
		int w1 = w1_row;
		int w2 = w2_row;

		for (p.x = minX; p.x <= maxX; p.x++) {
			// If p is on or inside all edges, render pixel.
    			if ((w0 | w1 | w2) >= 0) {
				plot(p.x, p.y, chan, color);
			}
			//renderPixel(p, w0, w1, w2);

			// One step to the right
			w0 += A12;
			w1 += A20;
			w2 += A01;
		}

		// One row step
		w0_row += B12;
		w1_row += B20;
		w2_row += B01;
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
