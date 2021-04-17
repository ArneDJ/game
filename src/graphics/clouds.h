// contains the parameters and creates the 3D textures

// contains the buffers and textures where the actual clouds are rendered to
// also does the drawing
class Clouds {
public:
	void init(int SW, int SH);
	void teardown(void);
	void gen_parameters(float covrg);
	void update(const Camera *camera, const glm::vec3 &lightpos, float time);
	void bind(GLenum unit) const;
private:
	int SCR_WIDTH, SCR_HEIGHT;
	//
	float coverage, speed;
	glm::vec3 wind_direction; 
	glm::vec3 topcolor, bottomcolor;
	glm::vec3 seed; 
	// 2D textures
	GLuint weathermap;
	GLuint cloudscape;
	GLuint blurred_cloudscape;
	// 3D textures
	GLuint perlin, worley32;
	//
	Shader volumetric;
	Shader weather;
	Shader perlinworley;
	Shader worley;
	Shader blurry; // to blur final texture
private:
	void init_shaders(void);
	void generate_noise(void);
};
