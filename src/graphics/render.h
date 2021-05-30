namespace GRAPHICS {

class FrameSystem {
public:
	FrameSystem(uint16_t w, uint16_t h);
	~FrameSystem(void);
	void copy_depthmap(void);
	GLuint get_depthmap_copy(void) const
	{
		return depthmap_copy;
	}
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
	GLuint depthmap_copy;
	GLsizei height;
	GLsizei width;
	// the screen mesh to render
	GLuint VAO = 0;
	GLuint VBO = 0;
	// final post process shader
	Shader postproc;
	Shader copy;
};

struct RenderObject {
	const Model *model;
	std::vector<const Entity*> entities;
	bool instanced = false;
	TransformBuffer tbuffer;
};

class Billboard {
public:
	Billboard(const Texture *tex, const Mesh *mesh) ;
	~Billboard(void);
	void clear(void);
	void instance(const std::vector<const Entity*> &ents);
	void display(GLuint unit) const;
private:
	const Texture *texture;
	const Mesh *quad;
	TransformBuffer tbuffer;
};

class BillboardGroup {
public:
	BillboardGroup(const Shader *shady);
	~BillboardGroup(void);
	void add_billboard(const Texture *tex, const std::vector<const Entity*> &ents);
	void display(const UTIL::Camera *camera) const;
	void clear(void);
private:
	const Shader *shader;
	std::vector<Billboard*> billboards;
	Mesh *quad;
};


class RenderGroup {
public:
	RenderGroup(const Shader *shady);
	~RenderGroup(void);
	void add_object(const Model *mod, const std::vector<const Entity*> &ents);
	void display(const UTIL::Camera *camera) const;
	void clear(void);
private:
	const Shader *shader;
	std::vector<RenderObject*> objects;
};

class RenderManager {
public:
	void init(uint16_t w, uint16_t h);
	void teardown(void);
	void bind_FBO(void);
	void bind_depthmap(GLuint unit);
	void final_render(void);
private:
	FrameSystem *framesystem;
};

};
