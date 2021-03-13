
// OpenGL texture
class Texture {
public:
	Texture(const std::string &filepath); // load image texture from file
	//Texture(const Image *image); // load image texture from memory
	~Texture(void);
private:
	GLuint handle = 0;
	GLenum target = GL_TEXTURE_2D; // usually GL_TEXTURE_2D
	GLsizei width = 0;
	GLsizei height = 0;
	GLsizei length = 0; // only for 3D textures
};
