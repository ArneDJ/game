
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
	void clear(void);
};

// mutable OpenGL texture
// class DynamicTexture

// to prevent duplicate textures
class TextureManager {
public:
	const Texture* add(const std::string &filepath);
	~TextureManager(void);
private:
	std::map<uint64_t, Texture*> textures;
};
