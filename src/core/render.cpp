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

#include "entity.h"
#include "shader.h"
#include "camera.h"
#include "image.h"
#include "texture.h"
#include "mesh.h"
#include "model.h"
#include "render.h"

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
