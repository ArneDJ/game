#pragma once
#include "../extern/cgltf/cgltf.h"

namespace GLTF {

struct node;

enum pathtype { TRANSLATION, ROTATION, SCALE }; 

struct material {
	GLuint colormap = 0;
	GLuint metalroughmap = 0;
	GLuint normalmap = 0;
	GLuint occlusionmap = 0;
	GLuint emissivemap = 0;
};

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

struct collision_mesh {
	std::vector<glm::vec3> positions;
	std::vector<uint16_t> indices;
};

class Model {
public:
	struct collision_mesh colmesh; // TODO multiple collision meshes
	std::vector<struct skin> skins; 
public:
	Model(const std::string &filepath, const std::string &diffusepath);
//	Model(const std::string &filepath);
	~Model(void);
	void display(void) const;
	void display_instanced(GLsizei count) const;
private:
	std::vector<struct node> nodes;
	std::vector<Mesh*> meshes;
	Texture diffuse;
	//std::vector<GLuint> textures;
	void load_data(const std::string &fpath, const cgltf_data *data);
	void load_mesh(const cgltf_mesh *gltfmesh);
};

}

glm::mat4 global_node_transform(const struct GLTF::node *n);

