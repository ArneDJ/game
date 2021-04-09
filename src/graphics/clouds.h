
class Cloudscape;

// contains the parameters and creates the 3D textures
class Clouds {
public:
	friend class Cloudscape;
	void init(void);
	void teardown(void);
private:
	Shader volumetric;
	Shader weather;
	float coverage, speed, crispiness, curliness, density, absorption;
	float earth_radius, inner_radius, outer_radius;
	float perlin_frequency;
	bool powdered;
	glm::vec3 topcolor, bottomcolor;
	glm::vec3 seed, old_seed;
	// 3D textures
	GLuint perlin, worley32, weathermap;
private:
	void init_variables(void);
	void init_shaders(void);
	void generate_textures(void);
	void generate_weather(void);
};

// contains the buffers and textures where the actual clouds are rendered to
// also does the drawing
class Cloudscape {
public:
	void init(int SW, int SH);
	void teardown(void);
	void update(const Camera *camera, const glm::vec3 &lightpos, float time);
	GLuint get_raw_clouds(void) const
	{
		return cloudscape;
	}
private:
	int SCR_WIDTH, SCR_HEIGHT;
	GLuint cloudscape;
	Clouds clouds;
};
