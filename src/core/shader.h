
struct shaderinfo {
	std::string filepath;
	GLenum type;
};

class Shader {
public:
	//Shader(const std::vector<struct shaderinfo> &info);
	~Shader(void);
	void compile(const std::string &filepath, GLenum type);
	void link(void);
	void use(void) const;
private:
	GLuint program = 0;
	std::vector<GLuint> shaders;
};
