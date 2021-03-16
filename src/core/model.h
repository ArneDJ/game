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

struct primitive {
	uint32_t firstindex;
	uint32_t indexcount;
	uint32_t firstvertex;
	uint32_t vertexcount;
	GLenum mode; // rendering mode (GL_TRIANGLES, GL_LINES, etc)
	//GLenum indextype; // (GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT)
	bool indexed;
	struct material mat;
};

struct mesh {
	std::string name;
	GLuint VAO, VBO, EBO;
 	std::vector<struct primitive> primitives;
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

struct animationchannel {
	enum pathtype path;
	std::vector<float> inputs;
	std::vector<glm::vec4> outputs;
	struct node *target;
};

struct animation {
	std::string name;
	std::vector<struct animationchannel> channels;
	float start = std::numeric_limits<float>::max();
	float end = std::numeric_limits<float>::min();
};

struct collision_mesh {
	std::vector<glm::vec3> positions;
	std::vector<uint16_t> indices;
};

class Model {
public:
	struct collision_mesh colmesh;
	std::vector<struct skin> skins;
public:
	Model(const std::string &filepath);
	~Model(void);
	//void animate(int index, float time);
	void display(void) const;
	void display_instanced(GLsizei count) const;
private:
	std::vector<struct node> nodes;
	std::vector<struct mesh> meshes;
	//std::vector<GLuint> textures;
	//std::vector<struct animation> animations;
	void load_data(const std::string &fpath, const cgltf_data *data);
};

}

glm::mat4 global_node_transform(const struct GLTF::node *n);

