
struct image_record {
	uint16_t width;
	uint16_t height;
	uint8_t channels;
	size_t size;
	std::vector<uint8_t> data;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(width), CEREAL_NVP(height), CEREAL_NVP(channels), CEREAL_NVP(size), CEREAL_NVP(data));
	}
};

struct floatimage_record {
	uint16_t width;
	uint16_t height;
	uint8_t channels;
	size_t size;
	std::vector<float> data;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(width), CEREAL_NVP(height), CEREAL_NVP(channels), CEREAL_NVP(size), CEREAL_NVP(data));
	}
};

// store navigation data
struct nav_tilemesh_record {
	int x;
	int y;
	std::vector<uint8_t> data;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(x, y, data);
	}
};

struct navmesh_record {
	glm::vec3 origin; 
	float tilewidth; 
	float tileheight; 
	int maxtiles; 
	int maxpolys;
	std::vector<struct nav_tilemesh_record> tilemeshes;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(origin.x, origin.y, origin.z, tilewidth, tileheight, maxtiles, maxpolys, tilemeshes);
	}
};

class Saver {
public:
	void change_directory(const std::string &dir);
	void save(const std::string &filepath, const Atlas *atlas, const Navigation *landnav, const long seed);
	void load(const std::string &filepath, Atlas *atlas, Navigation *landnav, long &seed);
private:
	std::string directory;
	struct floatimage_record topology;
	struct image_record watermap;
	struct image_record temperature;
	struct image_record rain;
	struct navmesh_record navmesh_land;
};
