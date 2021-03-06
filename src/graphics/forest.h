namespace gfx {

enum BVH_axis : uint8_t {
	BVH_AXIS_X = 0,
	BVH_AXIS_Y,
	BVH_AXIS_Z
};

struct tree_model_t {
	const Model *trunk;
	const Model *leaves;
	const Model *billboard;
	geom::AABB_t bounds;
	std::vector<geom::transformation_t> transformations;
	TransformBuffer detail_transforms;
	TransformBuffer billboard_transforms;
	uint32_t detail_count = 0;
	uint32_t billboard_count = 0;
};

struct tree_instance_t {
	tree_model_t *model;
	geom::transformation_t transform;
	glm::mat4 T;
};

struct BVH_node_t {
	geom::AABB_t bounds;
	BVH_node_t *left = nullptr;
	BVH_node_t *right = nullptr;
	std::vector<tree_instance_t> objects;
	BVH_node_t(const std::vector<tree_instance_t> &instances, enum BVH_axis parent_axis);
};

// bounding volume hierarchy
class BVH {
public:
	geom::AABB_t bounds;
	BVH_node_t *root = nullptr;
	std::vector<BVH_node_t*> nodes; // linear array of nodes
	std::vector<BVH_node_t*> leafs;
public:
	void build(const std::vector<tree_instance_t> &instances);
	~BVH();
	void clear();
};

class Forest {
public:
	Forest(const Shader *detail, const Shader *billboard);
	void add_model(const Model *trunk, const Model *leaves, const Model *billboard, const std::vector<const geom::transformation_t*> &transforms);
	void build_hierarchy();
	void set_atmosphere(const glm::vec3 &sun_position, const glm::vec3 &fog_color, float fog_factor, const glm::vec3 &ambiance);
	void display(const util::Camera *camera) const;
	void clear();
private:
	const Shader *m_detailed;
	const Shader *m_billboard;
	std::vector<std::unique_ptr<tree_model_t>> m_models;
	BVH m_bvh;
};

};
