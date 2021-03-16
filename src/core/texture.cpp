#include <string>
#include <iostream>
#include <map>
#include <functional>
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h> 

#define DDSKTX_IMPLEMENT
#include "../extern/ddsktx/dds-ktx.h"

#include "logger.h"
#include "image.h"
#include "texture.h"

static inline GLenum texture_format(ddsktx_format format);
static GLuint DDS_to_texture(const uint8_t *blob, const size_t size);
static GLuint uncompressed_2D_texture(const void *texels, GLsizei width, GLsizei height, GLenum internalformat, GLenum format, GLenum type);

Texture::Texture(const std::string &filepath)
{
	target = GL_TEXTURE_2D;
	handle = 0;

	FILE *file = fopen(filepath.c_str(), "rb");
	if (file == nullptr) {
		std::string err = "Texture load error: failed to open file " + filepath;
		write_log(LogType::ERROR, err);
		return;
	}

	fseek(file, 0L, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	uint8_t *buffer = new uint8_t[size];
	fread(buffer, sizeof *buffer, size, file);

	fclose(file);

	handle = DDS_to_texture(buffer, size);

	delete [] buffer;
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
	if (glIsTexture(handle) == GL_TRUE) {
		glDeleteTextures(1, &handle);
	}
}
	
void Texture::bind(GLenum unit) const
{
	glActiveTexture(unit);
	glBindTexture(target, handle);
}

TextureManager::~TextureManager(void)
{
	for (auto it = textures.begin(); it != textures.end(); it++) {
		Texture *texture = it->second;
		delete texture;
	}
}
	
const Texture* TextureManager::add(const std::string &filepath)
{
	// lookup the texture and see if its in the map
	uint64_t key = std::hash<std::string>()(filepath);
	auto mit = textures.find(key);

	// check if its not in the map
	if (mit == textures.end()) {
		// load the texture
		Texture *texture = new Texture { filepath };

		// insert it into the map
		textures.insert(std::make_pair(key, texture));

		return texture;
	}

	return mit->second;
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
