#include <vector>
#include <string>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/shader.h"
#include "core/camera.h"
#include "core/image.h"
#include "core/texture.h"
#include "core/mesh.h"
#include "worldmap.h"

static const uint32_t WORLDMAP_PATCH_RES = 85;

Worldmap::Worldmap(const glm::vec3 &mapscale, const FloatImage *heightmap)
{
	scale = mapscale;
	glm::vec2 min = { -5.f, -5.f };
	glm::vec2 max = { mapscale.x + 5.f, mapscale.z + 5.f };
	patches = new Mesh { WORLDMAP_PATCH_RES, min, max };

	topology = new Texture { heightmap };
	// special wrapping mode so edges of the map are at height 0
	topology->change_wrapping(GL_CLAMP_TO_EDGE);

	land.compile("shaders/worldmap.vert", GL_VERTEX_SHADER);
	land.compile("shaders/worldmap.tesc", GL_TESS_CONTROL_SHADER);
	land.compile("shaders/worldmap.tese", GL_TESS_EVALUATION_SHADER);
	land.compile("shaders/worldmap.frag", GL_FRAGMENT_SHADER);
	land.link();
}

void Worldmap::reload(const FloatImage *heightmap)
{
	topology->reload(heightmap);
}

void Worldmap::display(const Camera *camera)
{
	land.use();
	land.uniform_mat4("VP", camera->VP);
	land.uniform_vec3("CAM_POS", camera->position);
	land.uniform_vec3("MAP_SCALE", scale);

	topology->bind(GL_TEXTURE0);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
