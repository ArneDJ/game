namespace GRAPHICS {

// contains a set of random normalized points to be transformed for warparound grass
class GrassRoots {
public:
	GrassRoots(const Model *mod, uint32_t count);
	void display(void) const;
public:
	const Model *model;
	TransformBuffer tbuffer;
};

class GrassChunk {
public:
	GrassChunk(const GrassRoots *grassroots, const glm::vec3 &min, const glm::vec3 &max);
public:
	const GrassRoots *roots;
	struct AABB bbox;
	glm::vec2 offset;
	glm::vec2 scale;
};

class GrassSystem {
public:
	GrassSystem(const Model *mod);
	~GrassSystem(void);
	void refresh(const UTIL::Image<float> *heightmap, const glm::vec3 &scale);
	void colorize(const glm::vec3 &colr, const glm::vec3 &fogclr, const glm::vec3 &sun, float fogfctr);
	void display(const UTIL::Camera *camera, const glm::vec3 &scale) const;
private:
	std::vector<GrassRoots*> roots;
	std::vector<GrassChunk*> chunks;
private:
	Shader shader;
	glm::vec3 color;
	glm::vec3 fogcolor;
	glm::vec3 sunpos;
	float fogfactor;
};

class Terrain {
public:
	Terrain(const glm::vec3 &mapscale, const UTIL::Image<float> *heightmap, const UTIL::Image<uint8_t> *normalmap, const UTIL::Image<uint8_t> *cadastre);
	~Terrain(void);
	void add_material(const std::string &name, const Texture *texture);
	void reload(const UTIL::Image<float> *heightmap, const UTIL::Image<uint8_t> *normalmap, const UTIL::Image<uint8_t> *cadastre);
	void change_atmosphere(const glm::vec3 &sun, const glm::vec3 &fogclr, float fogfctr);
	void change_grass(const glm::vec3 &color);
	void display_land(const UTIL::Camera *camera) const;
	void display_water(const UTIL::Camera *camera, float time) const;
	void display_grass(const UTIL::Camera *camera) const;
private:
	std::unique_ptr<Mesh> patches;
	std::unique_ptr<GrassSystem> grass;
	std::unique_ptr<Texture> relief;
	std::unique_ptr<Texture> normals;
	std::unique_ptr<Texture> sitemasks;
	std::vector<texture_binding_t> materials;
	Shader land;
	Shader water;
	glm::vec3 scale;
private:
	// atmosphere
	glm::vec3 sunpos;
	float fogfactor;
	glm::vec3 fogcolor;
	glm::vec3 grasscolor;
};

};
