
class Terrain {
public:
	Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap);
	~Terrain(void);
	void reload(const FloatImage *heightmap);
	void display(const Camera *camera);
private:
	Image *normalmap = nullptr;
	Mesh *patches = nullptr;
	Texture *relief = nullptr;
	Texture *normals = nullptr;
	Shader land;
	glm::vec3 scale;
};
