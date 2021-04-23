
class TreeKind {
public:
	TreeKind(void);
	~TreeKind(void);
	void clear(void);
	void add(const glm::vec3 &position, const glm::quat &rotation);
public:
	std::vector<Entity*> entities;
};

class TreeForest {
public:
	void init(void);
	void teardown(void);
	void clear(void);
	void spawn(const glm::vec3 &mapscale, const FloatImage *heightmap, uint8_t precipitation);
public:
	const TreeKind* get_pine(void) const { return pine; };
private:
	TreeKind *pine; // testing with hardcoded tree
	Image *density;
};
