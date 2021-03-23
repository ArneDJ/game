
class Skybox {
public:
	void init(glm::vec3 top, glm::vec3 bottom);
	void teardown(void);
	void display(const Camera *camera);
private:
	glm::vec3 colortop, colorbottom;
	Shader shader;
	Mesh *cubemap = nullptr;
};
