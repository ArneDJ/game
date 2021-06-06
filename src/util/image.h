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

inline int min3(int a, int b, int c)
{
	return (std::min)(a, (std::min)(b, c));
}

inline int max3(int a, int b, int c)
{
	return (std::max)(a, (std::max)(b, c));
}

template <class T> class Image {
public:
	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t channels = 0;
	std::vector<T> data;
public:
	void resize(uint16_t w, uint16_t h, uint8_t chan)
	{
		width = w;
		height = h;
		channels = chan;

		data.clear();
		data.resize(width * height * channels);

		clear();
	}
	void copy(const Image<T> *original)
	{
		if (width != original->width || height != original->height || channels != original->channels || data.size() != original->data.size()) {
			width = original->width;
			height = original->height;
			channels = original->channels;

			data.clear();
			data.resize(original->data.size());
		}

		std::copy(original->data.begin(), original->data.end(), data.begin());
	}
	T sample(uint16_t x, uint16_t y, uint8_t chan) const
	{
		if (chan >= channels) { return 0; }

		if (x >= width || y >= height) { return 0; }

		const auto index = y * width * channels + x * channels + chan;

		if (index >= data.size()) { return 0; }

		return data[index];
	}
	void plot(uint16_t x, uint16_t y, uint8_t chan, T color)
	{
		if (chan >= channels) { return; }

		if (x >= width || y >= height) { return; }

		const auto index = y * width * channels + x * channels + chan;

		if (index >= data.size()) { return; }

		data[index] = color;
	}
	// wipe image clean but do not free memory
	void clear()
	{
		std::fill(data.begin(), data.end(), 0);
	}
	// line drawing
	void draw_line(int x0, int y0, int x1, int y1, uint8_t chan, T color)
	{
		int dx = abs(x1-x0), sx = x0 < x1 ? 1 : -1;
		int dy = -abs(y1-y0), sy = y0 < y1 ? 1 : -1;
		int err = dx+dy, e2; // error value e_xy

		for (;;) {
			plot(x0, y0, chan, color);
			if (x0 == x1 && y0 == y1) { break; }
			e2 = 2 * err;
			if (e2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0
			if (e2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0
		}
	}
	// triangle rasterize
	void draw_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, uint8_t chan, T color)
	{
		// make sure the triangle is counter clockwise
		if (clockwise(a, b, c)) {
			std::swap(b, c);
		}

		a.x = floorf(a.x);
		a.y = floorf(a.y);
		b.x = floorf(b.x);
		b.y = floorf(b.y);
		c.x = floorf(c.x);
		c.y = floorf(c.y);

		// this seems to fix holes
		draw_line(a.x, a.y, b.x, b.y, chan, color);
		draw_line(b.x, b.y, c.x, c.y, chan, color);
		draw_line(c.x, c.y, a.x, a.y, chan, color);

		// Compute triangle bounding box
		int minX = min3(int(a.x), int(b.x), int(c.x));
		int minY = min3(int(a.y), int(b.y), int(c.y));
		int maxX = max3(int(a.x), int(b.x), int(c.x));
		int maxY = max3(int(a.y), int(b.y), int(c.y));

		// Clip against screen bounds
		minX = (std::max)(minX, 0);
		minY = (std::max)(minY, 0);
		maxX = (std::min)(maxX, width - 1);
		maxY = (std::min)(maxY, height - 1);

		// Triangle setup
		int A01 = a.y - b.y, B01 = b.x - a.x;
		int A12 = b.y - c.y, B12 = c.x - b.x;
		int A20 = c.y - a.y, B20 = a.x - c.x;

		// Barycentric coordinates at minX/minY corner
		glm::ivec2 p = { minX, minY };
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
	void draw_filled_circle(int x0, int y0, int radius, uint8_t chan, T color)
	{
		int x = radius;
		int y = 0;
		int xchange = 1 - (radius << 1);
		int ychange = 0;
		int err = 0;

		while (x >= y) {
			for (int i = x0 - x; i <= x0 + x; i++) {
				plot(i, y0 + y, chan, color);
				plot(i, y0 - y, chan, color);
			}
			for (int i = x0 - y; i <= x0 + y; i++) {
				plot(i, y0 + x, chan, color);
				plot(i, y0 - x, chan, color);
			}

			y++;
			err += ychange;
			ychange += 2;
			if (((err << 1) + xchange) > 0) {
				x--;
				err += xchange;
				xchange += 2;
			}
		}
	}
	void draw_thick_line(int x0, int y0, int x1, int y1, int radius, uint8_t chan, T color)
	{
		int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
		int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
		int err = dx+dy, e2; // error value e_xy

		for (;;) {
			draw_filled_circle(x0,y0, radius, chan, color);
			if (x0 == x1 && y0 == y1) { break; }
			e2 = 2 * err;
			if (e2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0
			if (e2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0
		}
	}
public:
	// save to file
	void write(const std::string &filepath) const;
	// gaussian blur
	void blur(float sigma);
	// fill image with random noise at the specified channel
	void noise(FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t chan);
	void cellnoise(FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t chan);
	void create_normalmap(const Image<float> *displacement, float strength);
	void normalize(uint8_t chan);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(width, height, channels, data);
	}
};

};
