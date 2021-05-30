#pragma once
#include "../extern/fastnoise/FastNoise.h"

namespace UTIL {

enum color_channel : uint8_t {
	CHANNEL_RED = 0,
	CHANNEL_GREEN = 1,
	CHANNEL_BLUE = 2,
	CHANNEL_ALPHA = 3
};

enum colorspace_type : uint8_t {
	COLORSPACE_GRAYSCALE = 1,
	COLORSPACE_RGB = 3,
	COLORSPACE_RGBA = 4
};

class FloatImage;

class Image {
public:
	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t channels = 0;
	uint8_t *data = nullptr;
	size_t size = 0;
public:
	Image(const std::string &filepath);
	Image(uint16_t w, uint16_t h, uint8_t chan);
	~Image(void);
	void copy(const Image *original);
	uint8_t sample(uint16_t x, uint16_t y, uint8_t chan) const;
	void plot(uint16_t x, uint16_t y, uint8_t chan, uint8_t color);
	// gaussian blur
	void blur(float sigma);
	// wipe image clean but do not free memory
	void clear(void);
	// save to file
	void write(const std::string &filepath) const;
	// fill image with random noise at the specified channel
	void noise(FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t chan);
	// line drawing
	void draw_line(int x0, int y0, int x1, int y1, uint8_t chan, uint8_t color);
	// triangle rasterize
	void draw_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, uint8_t chan, uint8_t color);
	void draw_filled_circle(int x0, int y0, int radius, uint8_t chan, uint8_t color);
	void draw_thick_line(int x0, int y0, int x1, int y1, int radius, uint8_t chan, uint8_t color);
	void create_normalmap(const FloatImage *displacement, float strength);
private:
	bool malloced = false;
private:
};

// floating point image, used mostly for high precision heightmaps
class FloatImage {
public:
	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t channels = 0;
	float *data = nullptr;
	size_t size = 0;
public:
	FloatImage(uint16_t w, uint16_t h, uint8_t chan);
	~FloatImage(void);
	void copy(const FloatImage *original);
	float sample(uint16_t x, uint16_t y, uint8_t chan) const;
	void plot(uint16_t x, uint16_t y, uint8_t chan, float color);
	// gaussian blur
	void blur(float sigma);
	// wipe image clean but do not free memory
	void clear(void);
	// fill image with random noise at the specified channel
	void noise(FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t chan);
	void cellnoise(FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t chan);
	void normalize(uint8_t chan);
	void create_normalmap(const FloatImage *displacement, float strength);
};

};
