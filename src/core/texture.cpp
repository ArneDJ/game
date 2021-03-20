#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h> 

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#define DDSKTX_IMPLEMENT
#include "../extern/ddsktx/dds-ktx.h"

#include "logger.h"
#include "image.h"
#include "texture.h"

static inline GLenum texture_format(ddsktx_format format);
static GLuint load_DDS(const std::string &filepath);
static GLuint DDS_to_texture(const uint8_t *blob, const size_t size);
static GLuint uncompressed_2D_texture(const void *texels, GLsizei width, GLsizei height, GLenum internalformat, GLenum format, GLenum type);
	
Texture::Texture(void)
{
	target = GL_TEXTURE_2D;
	handle = 0;
}

Texture::Texture(const std::string &filepath)
{
	target = GL_TEXTURE_2D;
	handle = load_DDS(filepath);
}
	
// TODO handle 3D and texture arrays
Texture::Texture(const Image *image)
{
	target = GL_TEXTURE_2D;
	handle = 0;

	GLenum internalformat = 0;
	GLenum format = 0;
	switch (image->channels) {
	case 1: 
		internalformat = GL_R8; 
		format = GL_RED; 
		break;
	case 2: 
		internalformat = GL_RG8; 
		format = GL_RG; 
		break;
	case 3:
		internalformat = GL_RGB8; 
		format = GL_RGB; 
		break;
	case 4:
		internalformat = GL_RGBA8; 
		format = GL_RGBA; 
		break;
	}

	handle = uncompressed_2D_texture(image->data, image->width, image->height, internalformat, format, GL_UNSIGNED_BYTE);
}

Texture::~Texture(void)
{
	cleanup();
}
	
void Texture::cleanup(void)
{
	if (glIsTexture(handle) == GL_TRUE) {
		glDeleteTextures(1, &handle);
		handle = 0;
	}
}
	
void Texture::load(const std::string &filepath)
{
	// first delete texture data if it is present
	cleanup();

	// then create texture from DDS file
	handle = load_DDS(filepath);
}
	
void Texture::bind(GLenum unit) const
{
	glActiveTexture(unit);
	glBindTexture(target, handle);
}
	
void TransformBuffer::alloc(GLenum use)
{
	usage = use;
	size = matrices.size() * sizeof(glm::mat4);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_BUFFER, texture);

	glGenBuffers(1, &buffer);
	glBindBuffer(GL_TEXTURE_BUFFER, buffer);
	glBufferData(GL_TEXTURE_BUFFER, size, NULL, usage);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, buffer);
}

void TransformBuffer::update(void)
{
	glBindBuffer(GL_TEXTURE_BUFFER, buffer);
	glBufferData(GL_TEXTURE_BUFFER, size, matrices.data(), usage);
}

void TransformBuffer::bind(GLenum unit)
{
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_BUFFER, texture);
}

TransformBuffer::~TransformBuffer(void)
{
	if (glIsTexture(texture) == GL_TRUE) {
		glDeleteTextures(1, &texture);
		texture = 0;
	}
	if (glIsBuffer(buffer) == GL_TRUE) {
		glDeleteBuffers(1, &buffer);
		buffer = 0;
	}
}

static inline GLenum texture_format(ddsktx_format format)
{
	switch (format) {
	case DDSKTX_FORMAT_BC1: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; // DXT1
	case DDSKTX_FORMAT_BC2: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; // DXT3
	case DDSKTX_FORMAT_BC3: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; // DXT5
	// case DDSKTX_FORMAT_ETC1: return GL_ETC1_RGB8_OES;       // ETC1 RGB8 not recognized on Windows
	// case DDSKTX_FORMAT_ETC2: return GL_COMPRESSED_RGB8_ETC2;      // ETC2 RGB8 not recognized on Windows
	default : return 0;
	}
}

static GLuint load_DDS(const std::string &filepath)
{
	GLuint texture = 0;

	FILE *file = fopen(filepath.c_str(), "rb");
	if (!file) {
		std::string err = "Texture load error: failed to open file " + filepath;
		write_log(LogType::ERROR, err);
		return texture;
	}

	fseek(file, 0L, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	uint8_t *buffer = new uint8_t[size];
	fread(buffer, sizeof *buffer, size, file);

	fclose(file);

	texture = DDS_to_texture(buffer, size);

	delete [] buffer;

	return texture;
}

static GLuint DDS_to_texture(const uint8_t *blob, const size_t size)
{
	ddsktx_texture_info tc = {0};
	GLuint tex = 0;

	ddsktx_error error;
	if (ddsktx_parse(&tc, blob, size, &error)) {
		GLenum format = texture_format(tc.format);
		if (!format) { 
			write_log(LogType::ERROR, "DDS error: Invalid texture format");
			return 0; 
		}

		// Create GPU texture from tc data
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, tc.num_mips-1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		for (int mip = 0; mip < tc.num_mips; mip++) {
			ddsktx_sub_data sub_data;
			ddsktx_get_sub(&tc, &sub_data, blob, size, 0, 0, mip);
			if (sub_data.width <= 4 || sub_data.height <= 4) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip-1);
				break;
			}
			// Fill/Set texture sub resource data (mips in this case)
			glCompressedTexImage2D(GL_TEXTURE_2D, mip, format, sub_data.width, sub_data.height, 0, sub_data.size_bytes, sub_data.buff);
		}
	} else {
		std::string err = error.msg;
		write_log(LogType::ERROR, "DDS error: " + err);
	}

	return tex;
}

static GLuint uncompressed_2D_texture(const void *texels, GLsizei width, GLsizei height, GLenum internalformat, GLenum format, GLenum type)
{
	GLuint texture = 0;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, width, height);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, texels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}
