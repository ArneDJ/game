#include <chrono>
#include <iostream>
#include <cstring>

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
	size = w * h * chan;

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
	size = w * h * chan;
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
