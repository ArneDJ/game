
class Worldmap {
public:
	Worldmap(const glm::vec3 &mapscale, const FloatImage *heightmap);
	void reload(const FloatImage *heightmap);
	void display(const Camera *camera);
private:
	Mesh *patches = nullptr;
	Texture *topology = nullptr;
	Shader land;
	glm::vec3 scale;
};
