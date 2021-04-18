
// immutable OpenGL texture
class Texture {
public:
	Texture(void); // create an empty texture
	Texture(const std::string &filepath); // load image texture from file
	Texture(const Image *image); // load texture from image memory
	Texture(const FloatImage *image); // load texture from floating point image
	~Texture(void);
	// explicitly load texture
	void load(const std::string &filepath); // from file
	void bind(GLenum unit) const;
	// update texture data
	void reload(const FloatImage *image);
	void reload(const Image *image);
	void unload(FloatImage *image);
	// change wrapping method
	void change_wrapping(GLint wrapping);
	void change_filtering(GLint filter);
private:
	GLuint handle = 0;
	GLenum target = GL_TEXTURE_2D; // usually GL_TEXTURE_2D
	GLenum format = GL_RED;
private:
	void load_DDS(const std::string &filepath);
	void DDS_to_texture(const uint8_t *blob, const size_t size);
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

GLuint generate_2D_texture(const void *texels, GLsizei width, GLsizei height, GLenum internalformat, GLenum format, GLenum type);

GLuint generate_3D_texture(GLsizei width, GLsizei height, GLsizei depth, GLenum internalformat, GLenum format, GLenum type);

void delete_texture(GLuint handle);
