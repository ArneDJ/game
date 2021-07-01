namespace gfx {

// contains the buffers and textures where the actual clouds are rendered to
// also does the drawing
class Clouds {
public:
	void init(int SW, int SH);
	void teardown();
	void gen_parameters(float covrg);
	void update(const util::Camera *camera, const glm::vec3 &lightpos, float time);
	void bind(GLenum unit) const;
private:
	int m_screen_width = 0, m_screen_height = 0;
	//
	float m_coverage = 1.f, m_speed = 1.f;
	glm::vec3 m_wind_direction; 
	glm::vec3 m_top_color, m_bottom_color;
	glm::vec3 m_seed; 
	// 2D textures
	GLuint m_weathermap;
	GLuint m_cloudscape;
	GLuint m_blurred_cloudscape;
	// 3D textures
	GLuint m_perlin, m_worley32;
	//
	Shader m_volumetric;
	Shader m_weather;
	Shader m_perlinworley;
	Shader m_worley;
	Shader m_blurry; // to blur final texture
private:
	void init_shaders();
	void generate_noise();
};

};
