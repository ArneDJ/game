
// immutable OpenGL texture
class Texture {
public:
	Texture(void); // create an empty texture
	Texture(const std::string &filepath); // load image texture from file
	Texture(const Image *image); // load texture from image memory
	~Texture(void);
	// explicitly load texture
	void load(const std::string &filepath); // from file
	void bind(GLenum unit) const;
private:
	GLuint handle = 0;
	GLenum target = GL_TEXTURE_2D; // usually GL_TEXTURE_2D
private:
	void cleanup(void);
};

// mutable OpenGL texture
// class DynamicTexture

// https://www.khronos.org/opengl/wiki/Buffer_Texture
class TransformBuffer {
public:
	std::vector<glm::mat4> matrices;
public:
	void alloc(GLenum use);
	void update(void);
	void bind(GLenum unit);
	~TransformBuffer(void);
private:
	GLsizei size = 0;
	GLuint texture = 0;
	GLuint buffer = 0;
	GLenum usage = GL_STATIC_DRAW;
};

