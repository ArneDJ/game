
class Worldmap {
public:
	Worldmap(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *watermap, const Image *rainmap, const Image *materialmasks);
	~Worldmap(void);
	void load_materials(const std::vector<const Texture*> textures);
	void reload(const FloatImage *heightmap, const Image *watermap, const Image *rainmap, const Image *materialmasks);
	void change_atmosphere(const glm::vec3 &fogclr, float fogfctr);
	void display(const Camera *camera) const;
private:
	Mesh *patches = nullptr;
	Image *normalmap = nullptr;
	Texture *topology = nullptr;
	Texture *nautical = nullptr;
	Texture *rain = nullptr;
	Texture *normals = nullptr;
	Texture *masks = nullptr;
	std::vector<const Texture*> materials;
	Shader land;
	Shader water;
	glm::vec3 scale;
	// atmosphere
	float fogfactor;
	glm::vec3 fogcolor;
};
