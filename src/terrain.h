
class Terrain {
public:
	Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap);
	~Terrain(void);
	void reload(const FloatImage *heightmap);
	void display(const Camera *camera);
private:
	Mesh *patches = nullptr;
	Texture *relief = nullptr;
	Shader land;
	glm::vec3 scale;
};
