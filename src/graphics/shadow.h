#pragma once
#include <array> // has to be included here to compile on windows

namespace gfx {

struct depthmap {
	GLuint FBO;
	GLuint texture;
	GLsizei height;
	GLsizei width;
};

class Shadow {
public:
	Shadow(size_t texture_size);
	~Shadow(void);
	void update(const UTIL::Camera *camera, const glm::vec3 &lightpos);
	void enable(void);
	void bind_depthmap(uint8_t section);
	void disable(void);
	void bind_textures(GLenum unit) const;
	const std::vector<glm::mat4>& get_biased_shadowspaces(void) const
	{
		return biased_shadowspaces;
	}
	const glm::vec4& get_splitdepth(void) const
	{
		return splitdepth;
	}
	const std::array<glm::mat4, 4>& get_shadowspaces(void) const
	{
		return shadowspaces;
	}
private:
	std::array<glm::mat4, 4> shadowspaces;
	std::vector<glm::mat4> biased_shadowspaces;
	glm::vec4 splitdepth;
	struct depthmap depth;
};

};
