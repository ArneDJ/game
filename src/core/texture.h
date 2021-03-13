
// OpenGL texture
class Texture {
public:
	Texture(const std::string &filepath); // load image texture from file
	//Texture(const Image *image); // load image texture from memory
	~Texture(void);
	void bind(GLenum unit) const;
private:
	GLuint handle = 0;
	GLenum target = GL_TEXTURE_2D; // usually GL_TEXTURE_2D
};

// to prevent duplicate textures
class TextureManager {
public:
	const Texture* add(const std::string &filepath);
	~TextureManager(void);
private:
	std::map<uint64_t, Texture*> textures;
};
