namespace gfx {

class Skybox {
public:
	void init(uint16_t width, uint16_t height);
	void teardown(void);
	void prepare(void);
	void colorize(const glm::vec3 &top, const glm::vec3 &bottom, const glm::vec3 &sunpos, const glm::vec3 &ambiance, bool cloudsenabled);
	void update(const UTIL::Camera *camera, float time);
	void display(const UTIL::Camera *camera) const;
private:
	glm::vec3 zenith, horizon;
	glm::vec3 sunposition;
	glm::vec3 m_ambiance;
	float m_fog_factor = 1.f;
	glm::vec3 m_fog_color;
	bool clouds_enabled = false;
	bool clouded = false;
	Shader shader;
	Mesh *cubemap = nullptr;
	Clouds clouds;
};

};
