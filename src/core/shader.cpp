#include <iostream>
#include <fstream>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h>

#include "logger.h"
#include "shader.h"

Shader::~Shader(void)
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
		std::string err = "Shader compile error: failed to open file " + filepath;
		write_log(LogType::ERROR, err);
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
		glGetShaderInfoLog(shader, len, &len, &log[0]);
		std::string err(log.begin(), log.end());
		write_log(LogType::ERROR, "Shader compilation failed: " + err);

		// clean up shader
		glDeleteShader(shader);

		return;
	}

	shaders.push_back(shader);
}
	
void Shader::link(void)
{
	program = glCreateProgram(); // TODO do this in constructor?

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
		glGetProgramInfoLog(program, len, &len, &log[0]);
		std::string err(log.begin(), log.end());
		write_log(LogType::ERROR, "Shader program linking failed: " + err);

		for (GLuint object : shaders) {
			glDetachShader(program, object);
			glDeleteShader(object);
		}
		glDeleteProgram(program);
	}
}
	
void Shader::use(void) const
{
	glUseProgram(program);
}
