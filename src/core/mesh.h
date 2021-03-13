
struct vertex {
	glm::vec3 position;
	glm::vec2 texcoord;
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
	Mesh(const std::vector<glm::vec3> &positions, const std::vector<glm::vec2> &textcoords);
	~Mesh(void);
	void draw(void) const;
private:
	GLuint VAO = 0; // Vertex Array Object
	GLuint VBO = 0; // Vertex Buffer Object
	GLuint EBO = 0; // Element Buffer Object
	GLenum indextype = GL_UNSIGNED_BYTE; // (GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT)
	std::vector<struct primitive> primitives;
};
