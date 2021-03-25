
// only used for debugging
struct vertex {
	glm::vec3 position;
	glm::vec3 color;
};

// pure buffer in bytes
struct vertex_data {
	std::vector<uint8_t> positions;
	std::vector<uint8_t> normals;
	std::vector<uint8_t> texcoords;
	std::vector<uint8_t> joints;
	std::vector<uint8_t> weights;
};

struct primitive {
	GLuint firstindex = 0;
	GLsizei indexcount = 0;
	GLuint firstvertex = 0;
	GLsizei vertexcount = 0;
	GLenum mode = GL_TRIANGLES; // rendering mode (GL_TRIANGLES, GL_PATCHES, etc)
	bool indexed = false;
};

class Mesh {
public:
	Mesh(void);
	Mesh(const struct vertex_data *data, const std::vector<uint8_t> &indices, const std::vector<struct primitive> &primis);
	Mesh(const std::vector<struct vertex> &vertices, const std::vector<uint16_t> &indices, GLenum mode, GLenum usage);
	Mesh(const std::vector<glm::vec3> &positions, const std::vector<uint16_t> &indices);
	// tesselation mesh
	Mesh(uint32_t res, const glm::vec2 &min, const glm::vec2 &max);
	~Mesh(void);
	void draw(void) const;
	void draw_instanced(GLsizei count);
public:
	GLuint VAO = 0; // Vertex Array Object
	GLuint VBO = 0; // Vertex Buffer Object
	GLuint EBO = 0; // Element Buffer Object
	GLenum indextype = GL_UNSIGNED_BYTE; // (GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT)
	std::vector<struct primitive> primitives;
};
