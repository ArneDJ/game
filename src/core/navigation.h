enum sample_poly_areas {
	SAMPLE_POLYAREA_GROUND,
	SAMPLE_POLYAREA_WATER,
	SAMPLE_POLYAREA_ROAD,
	SAMPLE_POLYAREA_DOOR,
	SAMPLE_POLYAREA_GRASS,
	SAMPLE_POLYAREA_JUMP,
};

enum sample_poly_flags {
	SAMPLE_POLYFLAGS_WALK  = 0x01,  // Ability to walk (ground, grass, road)
	SAMPLE_POLYFLAGS_SWIM  = 0x02,  // Ability to swim (water).
	SAMPLE_POLYFLAGS_DOOR  = 0x04,  // Ability to move through doors.
	SAMPLE_POLYFLAGS_JUMP  = 0x08,  // Ability to jump.
	SAMPLE_POLYFLAGS_DISABLED = 0x10,  // Disabled polygon
	SAMPLE_POLYFLAGS_ALL  = 0xffff // All abilities.
};

struct polyresult {
	bool found;
	glm::vec3 position;
	dtPolyRef poly;
};

class Navigation
{
public:
	Navigation(void);
	~Navigation(void);
	
	bool build(std::vector<float> &vertices, std::vector<int> &indices);
	
	void find_path(glm::vec3 startpos, glm::vec3 endpos, std::vector<glm::vec3> &pathways);
	struct polyresult point_on_navmesh(glm::vec3 point);
public:
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

	float *verts = nullptr;
	int nverts;
	int *tris = nullptr;
	int ntris;
	
	rcChunkyTriMesh *chunky_mesh = nullptr;
private:
	void build_all_tiles(void);
	void remove_all_tiles(void);
};
