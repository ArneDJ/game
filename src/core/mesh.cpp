#include <iostream>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h> 
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "logger.h"
#include "mesh.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

static size_t typesize(GLenum type);

Mesh::Mesh(const std::vector<glm::vec3> &positions, const std::vector<uint16_t> &indices)
{
	const size_t position_size = sizeof(glm::vec3) * positions.size();
	const size_t indices_size = sizeof(uint16_t) * indices.size();

	// tell OpenGL how to render the buffer
	struct primitive primi;
	primi.firstindex = 0;
	primi.indexcount = GLsizei(indices.size());
	primi.firstvertex = 0;
	primi.vertexcount = GLsizei(positions.size());
	primi.mode = GL_TRIANGLES;
	primi.indexed = indices.size() > 0;

	primitives.push_back(primi);

	// create the OpenGL buffers
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// add index buffer
	if (primi.indexed) {
		glGenBuffers(1, &EBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices.data(), GL_STATIC_DRAW);
		indextype = GL_UNSIGNED_SHORT;
	}

	// add position buffer
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, position_size, positions.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
}
	
Mesh::Mesh(const std::vector<glm::vec3> &positions, const std::vector<glm::vec2> &texcoords)
{
	const size_t position_size = sizeof(glm::vec3) * positions.size();
	const size_t texcoord_size = sizeof(glm::vec2) * texcoords.size();

	// tell OpenGL how to render the buffer
	struct primitive primi = {
		0, 0, 0, GLsizei(positions.size()),
		GL_TRIANGLES, false
	};

	primitives.push_back(primi);

	// create the OpenGL buffers
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, position_size + texcoord_size, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, position_size, positions.data());
	glBufferSubData(GL_ARRAY_BUFFER, position_size, texcoord_size, texcoords.data());

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(position_size));
}

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
			glDrawElementsBaseVertex(prim.mode, prim.indexcount, indextype, (GLvoid *)((prim.firstindex)*typesize(indextype)), prim.firstvertex);
		} else {
			glDrawArrays(prim.mode, prim.firstvertex, prim.vertexcount);
		}
	}
}

static size_t typesize(GLenum type)
{
	switch (type) {
	case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
	case GL_UNSIGNED_SHORT: return sizeof(GLushort);
	case GL_UNSIGNED_INT: return sizeof(GLuint);
	};

	return 0;
}
