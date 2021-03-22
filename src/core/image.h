#pragma once
#include "../extern/fastnoise/FastNoise.h"

enum COLOR_CHANNEL : uint8_t {
	CHANNEL_RED = 0,
	CHANNEL_GREEN = 1,
	CHANNEL_BLUE = 2,
	CHANNEL_ALPHA = 3
};

enum COLORSPACE_TYPE : uint8_t {
	COLORSPACE_GRAYSCALE = 1,
	COLORSPACE_RGB = 3,
	COLORSPACE_RGBA = 4
};

class Image {
public:
	Image(const std::string &filepath);
	Image(uint16_t w, uint16_t h, uint8_t chan);
	~Image(void);
	uint16_t rows(void) const { return width; };
	uint16_t columns(void) const { return height; };
	void plot(uint16_t x, uint16_t y, uint8_t chan, uint8_t color);
	// gaussian blur
	void blur(float sigma);
	// wipe image clean but do not free memory
	void clear(void);
	// save to file
	void write(const std::string &filepath);
	// fill image with random noise at the specified channel
	void noise(FastNoise *fastnoise, const glm::vec2 &sample_freq, const glm::vec2 &sample_offset, uint8_t chan);
public:
	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t channels = 0;
	uint8_t *data = nullptr;
	size_t size = 0;
private:
	bool malloced = false;
};
