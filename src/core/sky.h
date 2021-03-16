
class Skybox {
public:
	Skybox(glm::vec3 top, glm::vec3 bottom);
	~Skybox(void);
	void display(const Camera *camera);
private:
	glm::vec3 colortop, colorbottom;
	Shader shader;
	Mesh *cubemap = nullptr;
};
