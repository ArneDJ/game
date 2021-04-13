
class FrameSystem {
public:
	FrameSystem(uint16_t w, uint16_t h);
	~FrameSystem(void);
	GLuint get_depthmap(void) const
	{
		return depthmap;
	}
	GLuint get_colormap(void) const
	{
		return colormap;
	}
	void bind_depthmap(GLenum unit) const;
	void bind_colormap(GLenum unit) const;
	void bind(void) const;
	void unbind(void) const;
	void display(void) const;
private:
	GLuint FBO;
	GLuint colormap;
	GLuint depthmap;
	GLsizei height;
	GLsizei width;
	// the screen mesh to render
	GLuint VAO = 0;
	GLuint VBO = 0;
	// final post process shader
	Shader shader;
};

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
