
struct debug_box {
	CubeMesh *mesh;
	std::vector<const Entity*> entities;
};

class Debugger {
public:
	void init(const Shader *shady);
	void teardown(void);
	// to visualize the navigation meshes
	void add_navmesh(const dtNavMesh *mesh);
	void render_navmeshes(void);
	void delete_navmeshes(void);
	// to visualize bounding boxes
	void add_bbox(const glm::vec3 &min, const glm::vec3 &max, const std::vector<const Entity*> &entities);
	void render_bboxes(const Camera *camera);
	void delete_bboxes(void);
private:
	const Shader *shader;
	std::vector<Mesh*> navmeshes;
	std::vector<debug_box> bboxes;
};

