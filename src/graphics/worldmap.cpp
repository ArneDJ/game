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

#include "../core/shader.h"
#include "../core/camera.h"
#include "../core/image.h"
#include "../core/texture.h"
#include "../core/mesh.h"
#include "worldmap.h"

static const uint32_t WORLDMAP_PATCH_RES = 85;

Worldmap::Worldmap(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *rainmap)
{
	scale = mapscale;
	glm::vec2 min = { -5.f, -5.f };
	glm::vec2 max = { mapscale.x + 5.f, mapscale.z + 5.f };
	patches = new Mesh { WORLDMAP_PATCH_RES, min, max };

	topology = new Texture { heightmap };
	// special wrapping mode so edges of the map are at height 0
	topology->change_wrapping(GL_CLAMP_TO_EDGE);

	rain = new Texture { rainmap };
	// special wrapping mode so edges of the map are at height 0
	rain->change_wrapping(GL_CLAMP_TO_EDGE);

	normalmap = new Image { heightmap->width, heightmap->height, COLORSPACE_RGB };
	normals = new Texture { normalmap };
	normals->change_wrapping(GL_CLAMP_TO_EDGE);

	land.compile("shaders/worldmap.vert", GL_VERTEX_SHADER);
	land.compile("shaders/worldmap.tesc", GL_TESS_CONTROL_SHADER);
	land.compile("shaders/worldmap.tese", GL_TESS_EVALUATION_SHADER);
	land.compile("shaders/worldmap.frag", GL_FRAGMENT_SHADER);
	land.link();

	water.compile("shaders/ocean.vert", GL_VERTEX_SHADER);
	water.compile("shaders/ocean.tesc", GL_TESS_CONTROL_SHADER);
	water.compile("shaders/ocean.tese", GL_TESS_EVALUATION_SHADER);
	water.compile("shaders/ocean.frag", GL_FRAGMENT_SHADER);
	water.link();
}
	
void Worldmap::load_materials(const std::vector<const Texture*> textures)
{
	materials.clear();
	materials.insert(materials.begin(), textures.begin(), textures.end());
}

Worldmap::~Worldmap(void)
{
	delete patches;
	
	delete topology;
	delete rain;

	delete normals;
	delete normalmap;
}

void Worldmap::reload(const FloatImage *heightmap, const Image *rainmap)
{
	topology->reload(heightmap);
	rain->reload(rainmap);
	normalmap->create_normalmap(heightmap, 32.f);
	normals->reload(normalmap);
}

void Worldmap::change_atmosphere(const glm::vec3 &fogclr, float fogfctr)
{
	fogcolor = fogclr;
	fogfactor = fogfctr;
}

void Worldmap::display(const Camera *camera) const
{
	land.use();
	land.uniform_mat4("VP", camera->VP);
	land.uniform_vec3("CAM_POS", camera->position);
	land.uniform_vec3("MAP_SCALE", scale);
	land.uniform_vec3("FOG_COLOR", fogcolor);
	land.uniform_float("FOG_FACTOR", fogfactor);

	topology->bind(GL_TEXTURE0);
	normals->bind(GL_TEXTURE1);
	rain->bind(GL_TEXTURE2);

	for (int i = 0; i < materials.size(); i++) {
		materials[i]->bind(GL_TEXTURE4 + i);
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	//
	water.use();
	water.uniform_mat4("VP", camera->VP);
	water.uniform_vec3("CAM_POS", camera->position);
	water.uniform_vec3("MAP_SCALE", scale);
	water.uniform_vec3("FOG_COLOR", fogcolor);
	water.uniform_float("FOG_FACTOR", fogfactor);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
