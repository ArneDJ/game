#include <vector>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "mesh.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

Mesh::~Mesh(void)
{
	glDeleteBuffers(1, &EBO);
	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);
}
	
void Mesh::draw(void) const
{
	glBindVertexArray(VAO);

	for (const auto &prim : primitives) {
		if (prim.indexed) {
			glDrawElementsBaseVertex(prim.mode, prim.indexcount, indextype, (GLvoid *)((prim.firstindex)*sizeof(GLushort)), prim.firstvertex);
		} else {
			glDrawArrays(prim.mode, prim.firstvertex, prim.vertexcount);
		}
	}
}

Mesh triangle_mesh(void)
{
	Mesh triangle;

	// create the buffers
	glGenVertexArrays(1, &triangle.VAO);
	glBindVertexArray(triangle.VAO);

	const std::vector<glm::vec3> vertices = {
		{ -0.5f, -0.5f, 0.0f }, 
		{ 0.5f, -0.5f, 0.0f },
		{ 0.0f,  0.5f, 0.0f }
	};

	glGenBuffers(1, &triangle.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, triangle.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*vertices.size(), vertices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0)); 
	glEnableVertexAttribArray(0);

	// tell OpenGL how to render the buffer
	struct primitive primi = {
		0, 0, 0, GLsizei(vertices.size()),
		GL_TRIANGLES, false
	};
	triangle.primitives.push_back(primi);

	return triangle;
}
