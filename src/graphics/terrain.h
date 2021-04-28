
// contains a set of random normalized points to be transformed for warparound grass
class GrassRoots {
public:
	GrassRoots(const GLTF::Model *mod, uint32_t count);
	void display(void) const;
public:
	const GLTF::Model *model;
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
	GrassSystem(const GLTF::Model *mod);
	~GrassSystem(void);
	void refresh(const FloatImage *heightmap, const glm::vec3 &scale);
	void colorize(const glm::vec3 &colr, const glm::vec3 &fogclr, const glm::vec3 &sun, float fogfctr);
	void display(const Camera *camera, const glm::vec3 &scale) const;
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
	Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *normalmap, const Image *cadastre, const GLTF::Model *grassmodel);
	~Terrain(void);
	void load_materials(const std::vector<const Texture*> textures);
	void reload(const FloatImage *heightmap, const Image *normalmap, const Image *cadastre);
	void change_atmosphere(const glm::vec3 &sun, const glm::vec3 &fogclr, float fogfctr);
	void change_grass(const glm::vec3 &color);
	void display_land(const Camera *camera) const;
	void display_water(const Camera *camera, float time) const;
	void display_grass(const Camera *camera) const;
private:
	Mesh *patches = nullptr;
	Texture *relief = nullptr;
	Texture *normals = nullptr;
	Texture *sitemasks = nullptr;
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
	//Grass *grass = nullptr;
	GrassSystem *grass = nullptr;
};
