
struct debug_box {
	gfx::CubeMesh *mesh;
	std::vector<const Entity*> entities;
};

struct debug_AABB_t {
	std::unique_ptr<gfx::CubeMesh> mesh;
	glm::vec3 color;
};

class Debugger {
public:
	void init(const gfx::Shader *shady);
	void teardown(void);
	// to visualize the navigation meshes
	void add_navmesh(const dtNavMesh *mesh);
	void render_navmeshes(void);
	void delete_navmeshes(void);
	// to visualize bounding boxes
	void add_bbox(const glm::vec3 &min, const glm::vec3 &max, const std::vector<const Entity*> &entities);
	void add_cube_mesh(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color);
	void render_bboxes(const UTIL::Camera *camera);
	void delete_bboxes(void);
private:
	const gfx::Shader *shader;
	std::vector<gfx::Mesh*> navmeshes;
	std::vector<debug_box> bboxes;
	std::vector<std::unique_ptr<debug_AABB_t>> cube_meshes;
};

