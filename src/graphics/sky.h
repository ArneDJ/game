
class Skybox {
public:
	void init(uint16_t width, uint16_t height);
	void teardown(void);
	void prepare(void);
	void colorize(const glm::vec3 &top, const glm::vec3 &bottom, const glm::vec3 &sunpos, bool cloudsenabled);
	void update(const Camera *camera, float time);
	void display(const Camera *camera) const;
private:
	glm::vec3 zenith, horizon;
	glm::vec3 sunposition;
	bool clouds_enabled = false;
	bool clouded = false;
	Shader shader;
	Mesh *cubemap = nullptr;
	Clouds clouds;
};
