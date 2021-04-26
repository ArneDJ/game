#pragma once
#include "../extern/cgltf/cgltf.h"

namespace GLTF {

struct node;

struct skin {
	std::string name;
	std::vector<glm::mat4> inversebinds;
	struct node *root; // skeleton root joint
	std::vector<struct node*> joints;
};

struct node {
	std::string name;
	glm::mat4 transform;
	glm::vec3 translation, scale;
	glm::quat rotation;
	struct node *parent = nullptr;
	std::vector<struct node*> children;
	struct skin *skeleton = nullptr;
};

// triangle mesh data
struct collision_trimesh {
	std::string name;
	std::vector<glm::vec3> positions;
	std::vector<uint16_t> indices;
};

// convex hull data
struct collision_hull {
	std::string name;
	std::vector<glm::vec3> points;
};

class Model {
public:
	std::vector<struct collision_trimesh> collision_trimeshes;
	std::vector<struct collision_hull> collision_hulls;
	std::vector<struct skin> skins; 
	glm::vec3 bound_min, bound_max; // model bounding box
public:
	Model(const std::string &filepath);
	~Model(void);
	void load_materials(const std::vector<const Texture*> textures);
	void display(void) const;
	void display_instanced(GLsizei count) const;
private:
	std::vector<struct node> nodes;
	std::vector<Mesh*> meshes;
	std::vector<const Texture*> materials;
	void load_data(const std::string &fpath, const cgltf_data *data);
	void load_mesh(const cgltf_mesh *gltfmesh);
	void find_bounds(const cgltf_data *data);
};

}

glm::mat4 global_node_transform(const struct GLTF::node *n);

