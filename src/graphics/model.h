#pragma once
#include "../extern/cgltf/cgltf.h"

namespace GRAPHICS {

struct node_t;

struct skin_t {
	std::string name;
	std::vector<glm::mat4> inversebinds;
	struct node_t *root; // skeleton root joint
	std::vector<struct node_t*> joints;
};

struct node_t {
	std::string name;
	glm::mat4 transform;
	glm::vec3 translation, scale;
	glm::quat rotation;
	struct node_t *parent = nullptr;
	std::vector<struct node_t*> children;
	struct skin_t *skeleton = nullptr;
};

// triangle mesh data
struct collision_trimesh_t {
	std::string name;
	std::vector<glm::vec3> positions;
	std::vector<uint16_t> indices;
};

// convex hull data
struct collision_hull_t {
	std::string name;
	std::vector<glm::vec3> points;
};

class Model {
public:
	std::vector<collision_trimesh_t> collision_trimeshes;
	std::vector<collision_hull_t> collision_hulls;
	std::vector<skin_t> skins; 
	glm::vec3 bound_min, bound_max; // model bounding box
public:
	Model(const std::string &filepath);
	~Model(void);
	void load_materials(const std::vector<const Texture*> textures);
	void display(void) const;
	void display_instanced(GLsizei count) const;
private:
	std::vector<struct node_t> nodes;
	std::vector<Mesh*> meshes;
	std::vector<const Texture*> materials;
	void load_data(const std::string &fpath, const cgltf_data *data);
	void load_mesh(const cgltf_mesh *gltfmesh);
	void find_bounds(const cgltf_data *data);
};

glm::mat4 global_node_transform(const struct node_t *n);

};
