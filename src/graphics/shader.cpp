#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/aixlog/aixlog.h"

#include "shader.h"

namespace gfx {

Shader::~Shader()
{
	// detach and delete shader objects
	for (GLuint object : shaders) {
		if (glIsShader(object) == GL_TRUE) {
			glDetachShader(program, object);
			glDeleteShader(object);
		}
	}

	// delete shader program
	if (glIsProgram(program) == GL_TRUE) { glDeleteProgram(program); }
}

void Shader::compile(const std::string &filepath, GLenum type)
{
	std::ifstream file(filepath);
        if (file.fail()) {
		LOG(ERROR, "Shader") << "Shader compile error: failed to open file " + filepath;
		return;
        }

	// read source file contents into memory
	std::string buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	file.close();

	const GLchar *source = buffer.c_str();

	// now create the OpenGL shader
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);

	glCompileShader(shader);

	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_FALSE) {
		// give the error
		GLsizei len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		std::vector<GLchar> log(len);
		glGetShaderInfoLog(shader, len, &len, log.data());
		std::string err(log.begin(), log.end());
		LOG(ERROR, "Shader") << "compilation failed: " + err;

		// clean up shader
		glDeleteShader(shader);

		return;
	}

	shaders.push_back(shader);
}
	
void Shader::link()
{
	program = glCreateProgram();

	// attach the shaders to the shader program
	for (GLuint object : shaders) {
		if (glIsShader(object) == GL_TRUE) {
			glAttachShader(program, object);
		}
	}

	glLinkProgram(program);

	// check if shader program was linked
	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLsizei len;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
		std::vector<GLchar> log(len);
		glGetProgramInfoLog(program, len, &len, log.data());
		std::string err(log.begin(), log.end());
		LOG(ERROR, "Shader") << "program linking failed: " + err;

		for (GLuint object : shaders) {
			glDetachShader(program, object);
			glDeleteShader(object);
		}
		glDeleteProgram(program);
	}
}
	
void Shader::use() const
{
	glUseProgram(program);
}

void Shader::uniform_bool(const GLchar *name, bool boolean) const
{
	glUniform1i(glGetUniformLocation(program, name), boolean);
}

void Shader::uniform_int(const GLchar *name, int integer) const
{
	glUniform1i(glGetUniformLocation(program, name), integer);
}

void Shader::uniform_float(const GLchar *name, GLfloat scalar) const
{
	glUniform1f(glGetUniformLocation(program, name), scalar);
}

void Shader::uniform_vec2(const GLchar *name, glm::vec2 vector) const
{
	glUniform2fv(glGetUniformLocation(program, name), 1, glm::value_ptr(vector));
}

void Shader::uniform_vec3(const GLchar *name, glm::vec3 vector) const
{
	glUniform3fv(glGetUniformLocation(program, name), 1, glm::value_ptr(vector));
}

void Shader::uniform_vec4(const GLchar *name, glm::vec4 vector) const
{
	glUniform4fv(glGetUniformLocation(program, name), 1, glm::value_ptr(vector));
}

void Shader::uniform_mat4(const GLchar *name, glm::mat4 matrix) const
{
	glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, glm::value_ptr(matrix));
}

void Shader::uniform_mat4_array(const GLchar *name, const std::vector<glm::mat4> &matrices) const
{
	glUniformMatrix4fv(glGetUniformLocation(program, name), matrices.size(), GL_FALSE, glm::value_ptr(matrices[0]));
}
	
GLint Shader::uniform_location(const GLchar *name) const
{
	return glGetUniformLocation(program, name);
}

};
