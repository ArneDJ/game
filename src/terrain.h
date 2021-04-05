
class Terrain {
public:
	Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *normalmap);
	~Terrain(void);
	void load_materials(const std::vector<const Texture*> textures);
	void reload(const FloatImage *heightmap, const Image *normalmap);
	void change_atmosphere(const glm::vec3 &fogclr, float fogfctr);
	void display(const Camera *camera);
private:
	Mesh *patches = nullptr;
	Texture *relief = nullptr;
	Texture *normals = nullptr;
	std::vector<const Texture*> materials;
	Shader land;
	glm::vec3 scale;
	// atmosphere
	float fogfactor;
	glm::vec3 fogcolor;
};
