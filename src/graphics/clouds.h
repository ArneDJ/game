
class Cloudscape;

// contains the parameters and creates the 3D textures
class Clouds {
public:
	friend class Cloudscape;
	void init(void);
	~Clouds(void);
private:
	Shader volumetric;
	Shader weather;
	float coverage, speed, crispiness, curliness, density, absorption;
	float earth_radius, sphere_inner_radius, sphere_outer_radius;
	float perlin_frequency;
	bool godrayed;
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
	~Cloudscape(void);
	void update(const Camera *camera, const glm::vec3 &lightpos, const glm::vec3 &zenith_color, const glm::vec3 &horizon_color, float time);
	enum cloudsTextureNames {fragColor, bloom, alphaness, cloudDistance};
	GLuint get_raw_clouds(void) const
	{
		return cloudsFBO->getColorAttachmentTex(0);
	}
	GLuint get_alpha_clouds(void) const
	{
		return cloudsFBO->getColorAttachmentTex(2);
	}
private:
	int SCR_WIDTH, SCR_HEIGHT;
	TextureSet *cloudsFBO;
	Clouds clouds;
};
