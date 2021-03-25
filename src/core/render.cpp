#include <vector>
#include <string>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "image.h"
#include "texture.h"
#include "mesh.h"
#include "render.h"
	
static const size_t WORLDMAP_PATCH_RES = 85;

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

	landshader.compile("shaders/worldmap.vert", GL_VERTEX_SHADER);
	landshader.compile("shaders/worldmap.tesc", GL_TESS_CONTROL_SHADER);
	landshader.compile("shaders/worldmap.tese", GL_TESS_EVALUATION_SHADER);
	landshader.compile("shaders/worldmap.frag", GL_FRAGMENT_SHADER);
	landshader.link();
}
	
void RenderManager::init_worldmap(const glm::vec3 &scale, const FloatImage *heightmap)
{
	mapscale = scale;
	glm::vec2 min = { -5.f, -5.f };
	glm::vec2 max = { scale.x + 5.f, scale.z + 5.f };
	patches = new Mesh { WORLDMAP_PATCH_RES, min, max };

	topology = new Texture { heightmap };
}
	
void RenderManager::reload_worldmap(const FloatImage *heightmap)
{
	topology->reload(heightmap);
}
	
void RenderManager::teardown(void)
{
	if (patches) {
		delete patches;
	}

	if (topology) {
		delete topology;
	}
}

void RenderManager::prepare_to_render(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderManager::display_worldmap(const Camera *camera)
{
	landshader.use();
	landshader.uniform_mat4("VP", camera->VP);
	landshader.uniform_vec3("CAM_POS", camera->position);
	landshader.uniform_vec3("MAP_SCALE", mapscale);

	topology->bind(GL_TEXTURE0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
