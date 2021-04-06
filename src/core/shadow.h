struct depthmap {
	GLuint FBO;
	GLuint texture;
	GLsizei height;
	GLsizei width;
};

class Shadow {
public:
	const uint8_t CASCADE_COUNT = 4;
	glm::mat4 shadowspace[4];
	glm::vec4 splitdepth;
public:
	Shadow(size_t texture_size);
	void update(const Camera *cam, glm::vec3 lightpos);
	void enable(void) const;
	void binddepth(unsigned int section) const;
	void disable(void) const;
	void bindtextures(GLenum unit) const;
	glm::mat4 biased_shadowspace(uint8_t cascade) const;
private:
	depthmap depth;
};
