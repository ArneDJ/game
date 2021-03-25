
class RenderManager {
public:
	void init(void);
	void init_worldmap(const glm::vec3 &scale, const FloatImage *heightmap);
	void reload_worldmap(const FloatImage *heightmap);
	void display_worldmap(const Camera *camera);
	void teardown(void);
	void prepare_to_render(void);
};
