
class Skybox {
public:
	void init(void);
	void teardown(void);
	void update(const glm::vec3 &top, const glm::vec3 &bottom, const glm::vec3 &sunpos, bool cloudsenable);
	void display(const Camera *camera) const;
private:
	glm::vec3 zenith, horizon;
	glm::vec3 sunposition;
	bool clouds_enabled = false;
	Shader shader;
	Mesh *cubemap = nullptr;
};
