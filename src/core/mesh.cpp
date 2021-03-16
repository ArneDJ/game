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
	
Mesh::Mesh(const struct vertex_data *data, const std::vector<uint8_t> &indices, const std::vector<struct primitive> &primis)
{
	indextype = GL_UNSIGNED_SHORT;

	primitives.insert(primitives.end(), primis.begin(), primis.end());

	std::vector<GLubyte> buffer;
	buffer.insert(buffer.end(), data->positions.begin(), data->positions.end());
	buffer.insert(buffer.end(), data->normals.begin(), data->normals.end());
	buffer.insert(buffer.end(), data->texcoords.begin(), data->texcoords.end());
	buffer.insert(buffer.end(), data->joints.begin(), data->joints.end());
	buffer.insert(buffer.end(), data->weights.begin(), data->weights.end());

	// https://www.khronos.org/opengl/wiki/Buffer_Object
	// In some cases, data stored in a buffer object will not be changed once it is uploaded. For example, vertex data can be static: set once and used many times.
	// For these cases, you set flags to 0 and use data as the initial upload. From then on, you simply use the data in the buffer. This requires that you have assembled all of the static data up-front.
	const GLbitfield flags = 0;

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, indices.size(), indices.data(), flags);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferStorage(GL_ARRAY_BUFFER, buffer.size(), buffer.data(), flags);

	// positions
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	// normals
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 0, BUFFER_OFFSET(data->positions.size()));
	glEnableVertexAttribArray(1);
	// texcoords
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(data->positions.size()+data->normals.size()));
	glEnableVertexAttribArray(2);
	// joints
	glVertexAttribIPointer(3, 4, GL_UNSIGNED_SHORT, 0, BUFFER_OFFSET(data->positions.size()+data->normals.size()+data->texcoords.size()));
	glEnableVertexAttribArray(3);
	// weights
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(data->positions.size()+data->normals.size()+data->texcoords.size()+data->joints.size()));
	glEnableVertexAttribArray(4);

}

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
	
Mesh::Mesh(const std::vector<glm::vec3> &positions, const std::vector<glm::vec2> &texcoords, GLenum mode)
{
	const size_t position_size = sizeof(glm::vec3) * positions.size();
	const size_t texcoord_size = sizeof(glm::vec2) * texcoords.size();

	// tell OpenGL how to render the buffer
	struct primitive primi = {
		0, 0, 0, GLsizei(positions.size()),
		mode, false
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
