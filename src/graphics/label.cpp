#include <vector>
#include <string>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>
#include <GL/gl.h> 

#include "../extern/freetype/freetype-gl.h"

#include "../core/text.h"
#include "../core/shader.h"
#include "../core/camera.h"
#include "label.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

LabelManager::LabelManager(const std::string &fontpath, size_t fontsize)
{
	atlas = texture_atlas_new(1024, 1024, 1);
	font = texture_font_new_from_file(atlas, fontsize, fontpath.c_str());

	// create the font atlas texture
	glGenTextures(1, &atlas->id);
	glBindTexture(GL_TEXTURE_2D, atlas->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas->width, atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, atlas->data);

	// create the batch mesh
	glGenVertexArrays(1, &glyph_batch.VAO);
	glBindVertexArray(glyph_batch.VAO);

	//GLuint EBO;
	glGenBuffers(1, &glyph_batch.EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glyph_batch.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t), NULL, GL_DYNAMIC_DRAW);

	//GLuint VBO;
	glGenBuffers(1, &glyph_batch.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, glyph_batch.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glyph_vertex_t), NULL, GL_DYNAMIC_DRAW);

	// positions
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), BUFFER_OFFSET(offsetof(glyph_vertex_t, position)));
	glEnableVertexAttribArray(0);
	// color
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), BUFFER_OFFSET(offsetof(glyph_vertex_t, color)));
	glEnableVertexAttribArray(1);
	// texcoords
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), BUFFER_OFFSET(offsetof(glyph_vertex_t, uv)));
	glEnableVertexAttribArray(2);

	shader.compile("shaders/label.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/label.frag", GL_FRAGMENT_SHADER);
	shader.link();
}

LabelManager::~LabelManager(void)
{
	// delete batch mesh
	glDeleteBuffers(1, &glyph_batch.EBO);
	glDeleteBuffers(1, &glyph_batch.VBO);
	glDeleteVertexArrays(1, &glyph_batch.VAO);

	glDeleteTextures(1, &atlas->id);

	texture_font_delete(font);

	texture_atlas_delete(atlas);
}
	
void LabelManager::add(const std::string &text, const glm::vec3 &color, const glm::vec3 &position)
{
	glm::vec2 pen = { 0.f, 0.f };

	glm::vec2 last = { color.x, color.z };

	struct glyph_buffer_t label;

	for (int i = 0; i < text.length(); i++) {
		texture_glyph_t *glyph = texture_font_get_glyph(font, &text.at(i));
		float kerning = 0.f;
		if (i > 0) {
			char back = text.at(i-1);
			kerning = texture_glyph_get_kerning(glyph, &text.at(i-1));
		}
		pen.x += kerning;
		int x0  = int(pen.x + glyph->offset_x);
		int y0  = int(pen.y + glyph->offset_y);
		int x1  = int(x0 + glyph->width);
		int y1  = int(y0 - glyph->height);
		float s0 = glyph->s0;
		float t0 = glyph->t0;
		float s1 = glyph->s1;
		float t1 = glyph->t1;

		uint32_t indices[6] = { 0, 1, 2, 0, 2, 3 };
		uint32_t offset = uint32_t(glyph_buffer.vertices.size()) + uint32_t(label.vertices.size());
		for (int j = 0; j < 6; j++) {
			label.indices.push_back(offset + indices[j]);
		}

		last = glm::vec2(x1, y1);

		glyph_vertex_t vertices[4] = { 
			{ { x0, y0, 0 }, position, { s0, t0 } },
			{ { x0, y1, 0 }, position, { s0, t1 } },
			{ { x1, y1, 0 }, position, { s1, t1 } },
			{ { x1, y0, 0 }, position, { s1, t0 } } 
		};
		for (int j = 0; j < 4; j++) {
			vertices[j].position *= glm::vec3(0.1f, 0.1f, 0.1f);
			label.vertices.push_back(vertices[j]);
		}
		pen.x += float(glyph->advance_x);
	}

	float halfwidth = 0.5f * last.x;
	for (int i = 0; i < label.vertices.size(); i++) {
		label.vertices[i].position.x -= 0.1f * halfwidth;
	}

	glyph_buffer.indices.insert(glyph_buffer.indices.end(), label.indices.begin(), label.indices.end());
	glyph_buffer.vertices.insert(glyph_buffer.vertices.end(), label.vertices.begin(), label.vertices.end());
}
	
void LabelManager::clear(void)
{
	glyph_buffer.vertices.clear();
	glyph_buffer.indices.clear();
}

void LabelManager::display(const Camera *camera) const
{
	shader.use();
	shader.uniform_mat4("PROJECT", camera->projection);
	shader.uniform_mat4("VIEW", camera->viewing);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, atlas->id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas->width, atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, atlas->data);

	glBindVertexArray(glyph_batch.VAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glyph_batch.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*glyph_buffer.indices.size(), glyph_buffer.indices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, glyph_batch.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glyph_vertex_t)*glyph_buffer.vertices.size(), glyph_buffer.vertices.data(), GL_DYNAMIC_DRAW);

	glDrawElements(GL_TRIANGLES, glyph_buffer.indices.size(), GL_UNSIGNED_INT, NULL);
}
