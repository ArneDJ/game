#pragma once
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/archives/binary.hpp"

struct image_record {
	uint16_t width;
	uint16_t height;
	uint8_t channels;
	size_t size;
	std::vector<uint8_t> data;

	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(CEREAL_NVP(width), CEREAL_NVP(height), CEREAL_NVP(channels), CEREAL_NVP(size), CEREAL_NVP(data));
	}
};

struct floatimage_record {
	uint16_t width;
	uint16_t height;
	uint8_t channels;
	size_t size;
	std::vector<float> data;

	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(CEREAL_NVP(width), CEREAL_NVP(height), CEREAL_NVP(channels), CEREAL_NVP(size), CEREAL_NVP(data));
	}
};

struct tile_record {
	// graph data
	uint32_t index;
	// world data
	bool frontier;
	bool land;
	bool coast;
	bool river;
	// graph data
	float center_x;
	float center_y;
	std::vector<uint32_t> neighbors;
	std::vector<uint32_t> corners;
	std::vector<uint32_t> borders;
	//
	//float amp;
	uint8_t relief;
	uint8_t biome;
	uint8_t site;
	int32_t holding = -1;

	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(CEREAL_NVP(index), CEREAL_NVP(frontier), CEREAL_NVP(land), CEREAL_NVP(coast), CEREAL_NVP(river), CEREAL_NVP(center_x), CEREAL_NVP(center_y), CEREAL_NVP(neighbors), CEREAL_NVP(corners), CEREAL_NVP(borders), CEREAL_NVP(relief), CEREAL_NVP(biome), CEREAL_NVP(site), CEREAL_NVP(holding));
	}
};

struct corner_record {
	// graph data
	uint32_t index;
	float position_x;
	float position_y;
	std::vector<uint32_t> adjacent;
	std::vector<uint32_t> touches;
	// world data
	bool frontier;
	bool coast;
	bool river;
	bool wall;
	int depth;
	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(CEREAL_NVP(index), CEREAL_NVP(position_x), CEREAL_NVP(position_y), CEREAL_NVP(adjacent), CEREAL_NVP(touches), CEREAL_NVP(frontier), CEREAL_NVP(coast), CEREAL_NVP(river), CEREAL_NVP(wall), CEREAL_NVP(depth));
	}
};

struct border_record {
	uint32_t index;
	uint32_t c0;
	uint32_t c1;
	uint32_t t0;
	uint32_t t1;
	// world data
	bool frontier;
	bool coast;
	bool river;
	bool wall;
	template <class Archive>
	void serialize(Archive &ar)
	{
		ar(CEREAL_NVP(index), CEREAL_NVP(c0), CEREAL_NVP(c1), CEREAL_NVP(t0), CEREAL_NVP(t1), CEREAL_NVP(frontier), CEREAL_NVP(coast), CEREAL_NVP(river), CEREAL_NVP(wall));
	}
};

class Saver {
public:
	void save(const std::string &filepath, const Atlas *atlas);
	void load(const std::string &filepath, Atlas *atlas);
private:
	struct floatimage_record topology;
	struct image_record temperature;
	struct image_record rain;
	// graph data
	std::vector<struct tile_record> tile_records;
	std::vector<struct corner_record> corner_records;
	std::vector<struct border_record> border_records;
	std::vector<struct tile> tiles;
	std::vector<struct corner> corners;
	std::vector<struct border> borders;
};
