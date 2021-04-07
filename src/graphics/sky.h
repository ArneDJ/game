
class Skybox {
public:
	void init(void);
	void teardown(void);
	void update(const glm::vec3 &top, const glm::vec3 &bottom, const glm::vec3 &sunpos);
	void display(const Camera *camera) const;
private:
	glm::vec3 colortop, colorbottom;
	glm::vec3 sunposition;
	Shader shader;
	Mesh *cubemap = nullptr;
};
