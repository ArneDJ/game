
struct RenderObject {
	const GLTF::Model *model;
	std::vector<const Entity*> entities;
};

class RenderGroup {
public:
	RenderGroup(const Shader *shady);
	~RenderGroup(void);
	void add_object(const GLTF::Model *mod, const std::vector<const Entity*> &ents);
	void display(const Camera *camera) const;
	// TODO
	void render(const Shader *program) const;
	void clear(void);
private:
	const Shader *shader;
	std::vector<RenderObject> objects;
};

class RenderManager {
public:
	void init(void);
	void teardown(void);
	void prepare_to_render(void);
};
