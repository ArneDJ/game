
class Worldmap {
public:
	Worldmap(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *rainmap);
	~Worldmap(void);
	void reload(const FloatImage *heightmap, const Image *rainmap);
	void display(const Camera *camera);
private:
	Mesh *patches = nullptr;
	Image *normalmap = nullptr;
	Texture *topology = nullptr;
	Texture *rain = nullptr;
	Texture *normals = nullptr;
	Shader land;
	glm::vec3 scale;
};
