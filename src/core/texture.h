
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
class TextureCache {
public:
	const Texture* add(const std::string &filepath);
	~TextureCache(void);
private:
	std::map<std::string, Texture*> textures; // TODO hash string
};
