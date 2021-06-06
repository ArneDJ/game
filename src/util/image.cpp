#include <chrono>
#include <iostream>
#include <cstring>
#include <vector>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/cereal/types/unordered_map.hpp"
#include "../extern/cereal/types/vector.hpp"
#include "../extern/cereal/types/memory.hpp"
#include "../extern/cereal/archives/binary.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stbimage/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../extern/stbimage/stb_image_write.h"

#include "../extern/fastgaussianblur/fast_gaussian_blur.h"
#include "../extern/fastgaussianblur/fast_gaussian_blur_template.h"

#include "../extern/aixlog/aixlog.h"

#include "geom.h"
#include "image.h"

namespace UTIL {

static glm::vec3 filter_normal(int x, int y, float strength, const Image<float> *image);

template<>
void Image<uint8_t>::blur(float sigma)
{
	uint8_t *blurred = new uint8_t[data.size()];
	uint8_t *input = data.data();

	fast_gaussian_blur_template(input, blurred, width, height, channels, sigma);

	std::swap(input, blurred);

	delete [] blurred;
}

template<>
void Image<float>::blur(float sigma)
{
	float *blurred = new float[data.size()];
	float *input = data.data();

	fast_gaussian_blur_template(input, blurred, width, height, channels, sigma);

	std::swap(input, blurred);

	delete [] blurred;
}
	
template<>
void Image<uint8_t>::noise(FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t chan)
{
	const int nsteps = 32;
	const int stepsize = width / nsteps;

	#pragma omp parallel for
	for (int step_x = 0; step_x < width; step_x += stepsize) {
		int w = step_x + stepsize;
		int h = height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				float x = sample_freq.x * j;
				float y = sample_freq.y * i;
				fastnoise->GradientPerturbFractal(x, y);
				float value = 0.5f * (fastnoise->GetNoise(x, y) + 1.f);
				plot(j, i, chan, 255 * glm::clamp(value, 0.f, 1.f));
			}
		}
	}
}

template<>
void Image<float>::noise(FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t chan)
{
	const int nsteps = 32;
	const int stepsize = width / nsteps;

	#pragma omp parallel for
	for (int step_x = 0; step_x < width; step_x += stepsize) {
		int w = step_x + stepsize;
		int h = height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				float x = sample_freq.x * j;
				float y = sample_freq.y * i;
				fastnoise->GradientPerturbFractal(x, y);
				float value = 0.5f * (fastnoise->GetNoise(x, y) + 1.f);
				plot(j, i, chan, glm::clamp(value, 0.f, 1.f));
			}
		}
	}
}

template<>
void Image<float>::normalize(uint8_t chan)
{
	const int nsteps = 32;
	const int stepsize = width / nsteps;

	// find min and max
	float min = (std::numeric_limits<float>::max)();
	float max = (std::numeric_limits<float>::min)();
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			float value = sample(x, y, chan);
			min = (std::min)(min, value);
			max = (std::max)(max, value);
		}
	}

	float scale = max - min;

	if (scale == 0.f) {
		return;
	}

	// now normalize the values
	#pragma omp parallel for
	for (int step_x = 0; step_x < width; step_x += stepsize) {
		int w = step_x + stepsize;
		int h = height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				float value = sample(j, i, chan);
				value = glm::clamp((value - min) / scale, 0.f, 1.f);
				plot(j, i, chan, value);
			}
		}
	}
}

template<>
void Image<float>::cellnoise(FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t chan)
{
	const int nsteps = 32;
	const int stepsize = width / nsteps;

	#pragma omp parallel for
	for (int step_x = 0; step_x < width; step_x += stepsize) {
		int w = step_x + stepsize;
		int h = height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				float x = sample_freq.x * j;
				float y = sample_freq.y * i;
				fastnoise->GradientPerturbFractal(x, y);
				plot(j, i, chan, fastnoise->GetNoise(x, y));
			}
		}
	}

	normalize(chan);
}

template<>
void Image<uint8_t>::create_normalmap(const Image<float> *displacement, float strength)
{
	if (channels != COLORSPACE_RGB) {
		LOG(ERROR, "Image") << "Normal map creation error: image is not RGB";
		return;
	}
	if (displacement->channels != COLORSPACE_GRAYSCALE) {
		LOG(ERROR, "Image") << "Normal map creation error: displacement image is not grayscale";
		return;
	}
	if (width != displacement->width || height != displacement->height) {
		LOG(ERROR, "Image") << "Normal map creation error: displacement image is not same resolution as normalmap image";
		return;
	}

	#pragma omp parallel for
	for (int x = 0; x < displacement->width; x++) {
		for (int y = 0; y < displacement->height; y++) {
			glm::vec3 normal = filter_normal(x, y, strength, displacement);
			// convert to positive values to store in an image
			normal.x = (normal.x + 1.f) / 2.f;
			normal.y = (normal.y + 1.f) / 2.f;
			normal.z = (normal.z + 1.f) / 2.f;
			plot(x, y, CHANNEL_RED, 255 * normal.x);
			plot(x, y, CHANNEL_GREEN, 255 * normal.y);
			plot(x, y, CHANNEL_BLUE, 255 * normal.z);
		}
	}
}

template<>
void Image<float>::create_normalmap(const Image<float> *displacement, float strength)
{
	if (channels != COLORSPACE_RGB) {
		LOG(ERROR, "Image") << "Normal map creation error: image is not RGB";
		return;
	}
	if (displacement->channels != COLORSPACE_GRAYSCALE) {
		LOG(ERROR, "Image") << "Normal map creation error: displacement image is not grayscale";
		return;
	}
	if (width != displacement->width || height != displacement->height) {
		LOG(ERROR, "Image") << "Normal map creation error: displacement image is not same resolution as normalmap image";
		return;
	}

	#pragma omp parallel for
	for (int x = 0; x < displacement->width; x++) {
		for (int y = 0; y < displacement->height; y++) {
			const glm::vec3 normal = filter_normal(x, y, strength, displacement);
			plot(x, y, CHANNEL_RED, normal.x);
			plot(x, y, CHANNEL_GREEN, normal.y);
			plot(x, y, CHANNEL_BLUE, normal.z);
		}
	}
}

template<>
void Image<uint8_t>::write(const std::string &filepath) const
{
	stbi_write_png(filepath.c_str(), width, height, channels, data.data(), channels*width);
}

static glm::vec3 filter_normal(int x, int y, float strength, const Image<float> *image)
{
	float T = image->sample(x, y + 1, CHANNEL_RED);
	float TR = image->sample(x + 1, y + 1, CHANNEL_RED);
	float TL = image->sample(x - 1, y + 1, CHANNEL_RED);
	float B = image->sample(x, y - 1, CHANNEL_RED);
	float BR = image->sample(x + 1, y - 1, CHANNEL_RED);
	float BL = image->sample(x - 1, y - 1, CHANNEL_RED);
	float R = image->sample(x + 1, y, CHANNEL_RED);
	float L = image->sample(x - 1, y, CHANNEL_RED);

	// sobel filter
	const float X = (TR + 2.f * R + BR) - (TL + 2.f * L + BL);
	const float Z = (BL + 2.f * B + BR) - (TL + 2.f * T + TR);
	const float Y = 1.f / strength;

	glm::vec3 normal(-X, Y, Z);
	normal = glm::normalize(normal);

	return normal;
}

};
