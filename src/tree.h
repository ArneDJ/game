
class TreeKind {
public:
	TreeKind(const GLTF::Model *modl);
	~TreeKind(void);
	void clear(void);
	void add(const glm::vec3 &position, const glm::quat &rotation);
public:
	const GLTF::Model* get_model(void) const { return model; };
	const std::vector<Entity*>& get_entities(void) const { return entities; };
private:
	const GLTF::Model *model;
	std::vector<Entity*> entities;
};

class TreeForest {
public:
	void init(const GLTF::Model *pinemodel);
	void teardown(void);
	void clear(void);
	void spawn(const glm::vec3 &mapscale, const FloatImage *heightmap, uint8_t precipitation);
public:
	const TreeKind* get_pine(void) const { return pine; };
private:
	TreeKind *pine; // testing with hardcoded tree
	Image *density;
};
