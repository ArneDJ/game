
class Terrain {
public:
	Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *normalmap);
	~Terrain(void);
	void load_materials(const std::vector<const Texture*> textures);
	void reload(const FloatImage *heightmap, const Image *normalmap);
	void change_atmosphere(const glm::vec3 &sun, const glm::vec3 &fogclr, float fogfctr);
	void update_shadow(const Shadow *shadow, bool show_cascades);
	void display_land(const Camera *camera) const;
	void display_water(const Camera *camera, float time) const;
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
};
