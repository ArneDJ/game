#include <iostream>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/camera.h"
#include "core/shader.h"
#include "core/mesh.h"
#include "sky.h"

void Skybox::init(void)
{
	// create the cubemap mesh
	const std::vector<glm::vec3> positions = {
		{ -1.0, -1.0,  1.0 },
		{ 1.0, -1.0,  1.0 },
		{ 1.0,  1.0,  1.0 },
		{ -1.0,  1.0,  1.0 },
		{ -1.0, -1.0, -1.0 },
		{ 1.0, -1.0, -1.0 },
		{ 1.0,  1.0, -1.0 },
		{ -1.0,  1.0, -1.0 }
	};
	// indices
	const std::vector<uint16_t> indices = {
		2, 1, 0, 0, 3, 2,
		6, 5, 1, 1, 2, 6,
		5, 6, 7, 7, 4, 5,
		3, 0, 4, 4, 7, 3,
		1, 5, 4, 4, 0, 1,
		6, 2, 3, 3, 7, 6
	};

	cubemap = new Mesh { positions, indices };

	shader.compile("shaders/skybox.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/skybox.frag", GL_FRAGMENT_SHADER);
	shader.link();
}

void Skybox::teardown(void)
{
	delete cubemap;
}
	
void Skybox::update(const glm::vec3 &top, const glm::vec3 &bottom, const glm::vec3 &sunpos)
{
	colortop = top;
	colorbottom = bottom;
	sunposition = sunpos;
}
	
void Skybox::display(const Camera *camera) const
{
	shader.use();
	shader.uniform_vec3("COLOR_TOP", colortop);
	shader.uniform_vec3("COLOR_BOTTOM", colorbottom);
	shader.uniform_vec3("SUN_POS", sunposition);
	shader.uniform_mat4("V", camera->viewing);
	shader.uniform_mat4("P", camera->projection);

	glDepthFunc(GL_LEQUAL);
	cubemap->draw();
	glDepthFunc(GL_LESS);
}
