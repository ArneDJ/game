
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
	void uniform_bool(const GLchar *name, bool boolean) const;
	void uniform_float(const GLchar *name, GLfloat scalar) const;
	void uniform_vec3(const GLchar *name, glm::vec3 vector) const;
	void uniform_mat4(const GLchar *name, glm::mat4 matrix) const;
private:
	GLuint program = 0;
	std::vector<GLuint> shaders;
};
