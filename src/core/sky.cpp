#include <iostream>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "mesh.h"
#include "sky.h"

Skybox::Skybox(glm::vec3 top, glm::vec3 bottom)
{
	colortop = top;
	colorbottom = bottom;

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

Skybox::~Skybox(void)
{
	delete cubemap;
}
	
void Skybox::display(glm::mat4 view, glm::mat4 project)
{
	shader.use();
	shader.uniform_vec3("COLOR_TOP", colortop);
	shader.uniform_vec3("COLOR_BOTTOM", colorbottom);
	shader.uniform_mat4("V", view);
	shader.uniform_mat4("P", project);

	glDepthFunc(GL_LEQUAL);
	cubemap->draw();
	//glBindVertexArray(cube.VAO);
	//glDrawElements(cube.mode, cube.ecount, GL_UNSIGNED_SHORT, NULL);
	glDepthFunc(GL_LESS);
}
