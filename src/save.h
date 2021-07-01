
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
	void save(const std::string &filename, const geography::Atlas &atlas, const util::Navigation *landnav, const util::Navigation *seanav, const long seed);
	void load(const std::string &filename, geography::Atlas &atlas, util::Navigation *landnav, util::Navigation *seanav, long &seed);
private:
	std::string directory;
	struct navmesh_record navmesh_land;
	struct navmesh_record navmesh_sea;
};
