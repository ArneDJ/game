
class GrassBox {
public:
	GrassBox(const GLTF::Model *mod, const struct rectangle &bounds);
	void generate(void);
	void place(const glm::vec3 &scale, const FloatImage *heightmap, const Image *normalmap);
	void display(void) const;
	const struct AABB* boundbox(void) const { return &box; }
private:
	uint32_t instance_count = 0;
	struct AABB box;
	struct rectangle boundaries;
	const GLTF::Model *model;
	TransformBuffer tbuffer;
	std::vector<glm::mat4> transforms;
};

class Grass {
public:
	Grass(const GLTF::Model *mod);
	~Grass(void);
	void colorize(const glm::vec3 &colr, const glm::vec3 &fogclr, const glm::vec3 &sun, float fogfctr);
	void generate(void);
	void place(const glm::vec3 &scale, const FloatImage *heightmap, const Image *normalmap);
	void display(const Camera *camera, const glm::vec3 &scale) const;
private:
	std::vector<GrassBox*> grassboxes;
private:
	Shader shader;
	glm::vec3 color;
	glm::vec3 fogcolor;
	glm::vec3 sunpos;
	float fogfactor;
};

class Terrain {
public:
	Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *normalmap, const GLTF::Model *grassmodel);
	~Terrain(void);
	void prepare(void);
	void load_materials(const std::vector<const Texture*> textures);
	void reload(const FloatImage *heightmap, const Image *normalmap);
	void change_atmosphere(const glm::vec3 &sun, const glm::vec3 &fogclr, float fogfctr);
	void change_grass(const glm::vec3 &color);
	void update_shadow(const Shadow *shadow, bool show_cascades);
	void display_land(const Camera *camera) const;
	void display_water(const Camera *camera, float time) const;
	void display_grass(const Camera *camera) const;
private:
	Mesh *patches = nullptr;
	Texture *relief = nullptr;
	Texture *normals = nullptr;
	std::vector<const Texture*> materials;
	Shader land;
	Shader water;
	glm::vec3 scale;
	// atmosphere
	glm::vec3 sunpos;
	float fogfactor;
	glm::vec3 fogcolor;
	glm::vec3 grasscolor;
	//
	Grass *grass = nullptr;
};
