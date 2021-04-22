
class TreeKind {
public:
	TreeKind(const GLTF::Model *modl, const GLTF::Model *board);
	~TreeKind(void);
	void clear(void);
	void prepare(void);
	void add(const glm::vec3 &position, const glm::quat &rotation);
	void display(uint32_t instancecount, uint32_t billboardcount);
public:
	const GLTF::Model *model;
	const GLTF::Model *billboard;
	std::vector<Entity*> entities;
	TransformBuffer tbuffer;
	TransformBuffer boardbuffer;
};

struct TreeBox {
	//struct AABB box;
	glm::vec3 min;
	glm::vec3 max;
	std::vector<Entity*> entities;
};

class TreeForest {
public:
	void init(const GLTF::Model *pinemodel, const GLTF::Model *pine_billboard);
	void teardown(void);
	void clear(void);
	void spawn(const glm::vec3 &mapscale, const FloatImage *heightmap, uint8_t precipitation);
	void colorize(const glm::vec3 &sunpos, const glm::vec3 &fog, float fogfact);
	void display(const Camera *camera) const;
public:
	const TreeKind* get_pine(void) const { return pine; };
private:
	TreeKind *pine; // testing with hardcoded tree
	Image *density;
	std::vector<struct TreeBox> boxes;
private:
	Shader shader;
	glm::vec3 sun_position;
	glm::vec3 fog_color;
	float fog_factor;
};
