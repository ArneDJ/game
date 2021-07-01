namespace util {

enum sample_poly_areas {
	SAMPLE_POLYAREA_GROUND,
	SAMPLE_POLYAREA_WATER,
	SAMPLE_POLYAREA_ROAD,
	SAMPLE_POLYAREA_DOOR,
	SAMPLE_POLYAREA_GRASS,
	SAMPLE_POLYAREA_JUMP,
};

enum sample_poly_flags {
	SAMPLE_POLYFLAGS_WALK = 0x01,  // Ability to walk (ground, grass, road)
	SAMPLE_POLYFLAGS_SWIM = 0x02,  // Ability to swim (water).
	SAMPLE_POLYFLAGS_DOOR = 0x04,  // Ability to move through doors.
	SAMPLE_POLYFLAGS_JUMP = 0x08,  // Ability to jump.
	SAMPLE_POLYFLAGS_DISABLED = 0x10,  // Disabled polygon
	SAMPLE_POLYFLAGS_ALL = 0xffff // All abilities.
};

struct poly_result_t {
	bool found = false;
	glm::vec3 position = {};
	dtPolyRef poly;
};

class Navigation
{
public:
	Navigation();
	~Navigation();
	const dtNavMesh* get_navmesh() const { return navmesh; };	
	bool alloc(const glm::vec3 &origin, float tilewidth, float tileheight, int maxtiles, int maxpolys);
	void cleanup();
	bool build(const std::vector<float> &vertices, const std::vector<int> &indices);
	void load_tilemesh(int x, int y, const std::vector<uint8_t> &data);
	
	void find_2D_path(const glm::vec2 &startpos, const glm::vec2 &endpos, std::list<glm::vec2> &pathways) const;
	void find_3D_path(const glm::vec3 &startpos, const glm::vec3 &endpos, std::vector<glm::vec3> &pathways) const;
	poly_result_t point_on_navmesh(const glm::vec3 &point) const;
private:
	dtNavMesh *navmesh = nullptr;
 	dtNavMeshQuery *navquery = nullptr;
private:
	rcConfig cfg;	
	int max_tiles;
	int max_polys_per_tile;
	int tile_tri_count;
	rcContext *context = nullptr;
	float BOUNDS_MIN[3];
	float BOUNDS_MAX[3];

	std::vector<float> verts;
	std::vector<int> tris;
	
	rcChunkyTriMesh *chunky_mesh = nullptr;
private:
	void build_all_tiles();
	void remove_all_tiles();
};

};
