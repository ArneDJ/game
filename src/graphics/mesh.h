namespace GRAPHICS {

// only used for debugging
struct vertex_t {
	glm::vec3 position;
	glm::vec3 color;
};

// pure buffer in bytes
struct vertex_data_t {
	std::vector<uint8_t> positions;
	std::vector<uint8_t> normals;
	std::vector<uint8_t> texcoords;
	std::vector<uint8_t> joints;
	std::vector<uint8_t> weights;
};

struct primitive_t {
	GLuint firstindex = 0;
	GLsizei indexcount = 0;
	GLuint firstvertex = 0;
	GLsizei vertexcount = 0;
	GLenum mode = GL_TRIANGLES; // rendering mode (GL_TRIANGLES, GL_PATCHES, etc)
	bool indexed = false;
};

class Mesh {
public:
	Mesh();
	Mesh(const vertex_data_t *data, const std::vector<uint8_t> &indices, const std::vector<primitive_t> &primis);
	Mesh(const std::vector<vertex_t> &vertices, const std::vector<uint16_t> &indices, GLenum mode, GLenum usage);
	Mesh(const std::vector<glm::vec3> &positions, const std::vector<uint16_t> &indices);
	Mesh(const std::vector<glm::vec3> &positions, const std::vector<glm::vec2> &texcoords, const std::vector<uint16_t> &indices);
	// tesselation mesh
	Mesh(uint32_t res, const glm::vec2 &min, const glm::vec2 &max);
	~Mesh(void);
	void draw(void) const;
	void draw_instanced(GLsizei count) const;
public:
	GLuint VAO = 0; // Vertex Array Object
	GLuint VBO = 0; // Vertex Buffer Object
	GLuint EBO = 0; // Element Buffer Object
	GLenum indextype = GL_UNSIGNED_BYTE; // (GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT)
	std::vector<primitive_t> primitives;
};

class CubeMesh : public Mesh {
public:
	CubeMesh(const glm::vec3 &min, const glm::vec3 &max);
};

};
