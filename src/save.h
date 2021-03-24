#pragma once
#include "extern/cereal/types/unordered_map.hpp"
#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"
#include "extern/cereal/archives/binary.hpp"

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

class Saver {
public:
	void save(const std::string &filepath, const Terragen *terra);
	void load(const std::string &filepath, Terragen *terra);
private:
	struct floatimage_record topology;
	struct image_record temperature;
	struct image_record rain;
};
