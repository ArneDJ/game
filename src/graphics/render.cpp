#include <vector>
#include <string>
#include <map>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../core/logger.h"
#include "../core/entity.h"
#include "../core/shader.h"
#include "../core/camera.h"
#include "../core/image.h"
#include "../core/texture.h"
#include "../core/mesh.h"
#include "../core/model.h"
#include "render.h"

#define INT_CEIL(n,d) (int)ceil((float)n/d)

RenderGroup::RenderGroup(const Shader *shady)
{
	shader = shady;
}

RenderGroup::~RenderGroup(void)
{
	clear();
}

void RenderGroup::add_object(const GLTF::Model *mod, const std::vector<const Entity*> &ents)
{
	struct RenderObject object;
	object.model = mod;
	object.entities.insert(object.entities.begin(), ents.begin(), ents.end());

	objects.push_back(object);
}

void RenderGroup::display(const Camera *camera) const
{
	shader->use();
	shader->uniform_mat4("VP", camera->VP);

	shader->uniform_bool("INSTANCED", false);

	for (const auto &obj : objects) {
		for (const auto &ent : obj.entities) {
			glm::mat4 T = glm::translate(glm::mat4(1.f), ent->position);
			glm::mat4 R = glm::mat4(ent->rotation);
			glm::mat4 M = T * R;
			glm::mat4 MVP = camera->VP * M;
			shader->uniform_mat4("MVP", MVP);
			shader->uniform_mat4("MODEL", M);
			obj.model->display();
		}
	}
}

void RenderGroup::clear(void)
{
	objects.clear();
}
	
void RenderManager::init(void)
{
	// set OpenGL states
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glLineWidth(5.f);
}
	
/*
void RenderManager::teardown(void)
{
}
*/

void RenderManager::prepare_to_render(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

FrameSystem::FrameSystem(uint16_t w, uint16_t h)
{
	width = w;
	height = h;

	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	// create the colormap texture
	glGenTextures(1, &colormap);
	glBindTexture(GL_TEXTURE_2D, colormap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// attach color texture to FBO
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colormap, 0);

	// create the depthmap texture
	glGenTextures(1, &depthmap);
	glBindTexture(GL_TEXTURE_2D, depthmap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	// GL_NEAREST seems to cause horizontal white lines
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// attach depth texture as FBO's depth buffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthmap, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		write_log(LogType::ERROR, "Frame system FBO error: incomplete FBO");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	glGenTextures(1, &depthmap_copy);
	glBindTexture(GL_TEXTURE_2D, depthmap_copy);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RED, GL_FLOAT, NULL);
	glBindTexture(GL_TEXTURE_2D, depthmap_copy);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);


	// create the screen quad mesh
	const float vertices[] = { 
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		1.0f, -1.0f,  1.0f, 0.0f,

		-1.0f,  1.0f,  0.0f, 1.0f,
		1.0f, -1.0f,  1.0f, 0.0f,
		1.0f,  1.0f,  1.0f, 1.0f
	};
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	// create the post processing shader
	postproc.compile("shaders/postproc.vert", GL_VERTEX_SHADER);
	postproc.compile("shaders/postproc.frag", GL_FRAGMENT_SHADER);
	postproc.link();

	copy.compile("shaders/copy.comp", GL_COMPUTE_SHADER);
	copy.link();
}

FrameSystem::~FrameSystem(void)
{
	delete_texture(depthmap);
	delete_texture(depthmap_copy);
	delete_texture(colormap);

	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);

	glDeleteFramebuffers(1, &FBO);
}

void FrameSystem::bind(void) const
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void FrameSystem::unbind(void) const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameSystem::display(void) const
{
	postproc.use();

	bind_colormap(GL_TEXTURE0);
	bind_depthmap(GL_TEXTURE1);

	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}
	
void FrameSystem::bind_colormap(GLenum unit) const
{
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D, colormap);
}

void FrameSystem::bind_depthmap(GLenum unit) const
{
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D, depthmap);
}
	
void FrameSystem::copy_depthmap(void)
{
	copy.use();
	copy.uniform_float("TEXTURE_WIDTH", float(width));
	copy.uniform_float("TEXTURE_HEIGHT", float(height));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthmap);
	glBindImageTexture(1, depthmap_copy, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
