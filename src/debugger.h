
struct debug_AABB_t {
	std::unique_ptr<gfx::CubeMesh> mesh;
	glm::vec3 color;
};

class Debugger {
public:
	void init(const gfx::Shader *shady);
	void teardown();
	// to visualize the navigation meshes
	void add_navmesh(const dtNavMesh *mesh);
	void render_navmeshes();
	void delete_navmeshes();
	// to visualize bounding boxes
	void add_cube_mesh(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color);
	void delete_boxes();
private:
	const gfx::Shader *shader;
	std::vector<gfx::Mesh*> navmeshes;
	std::vector<std::unique_ptr<debug_AABB_t>> cube_meshes;
};

