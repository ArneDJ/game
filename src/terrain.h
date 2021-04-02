
class Terrain {
public:
	Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *normalmap);
	~Terrain(void);
	void reload(const FloatImage *heightmap, const Image *normalmap);
	void display(const Camera *camera, const glm::vec3 &fogcolor, float fogfactor);
private:
	Mesh *patches = nullptr;
	Texture *relief = nullptr;
	Texture *normals = nullptr;
	Shader land;
	glm::vec3 scale;
};
