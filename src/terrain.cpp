#include <iostream>
#include <vector>
#include <string>
#include <chrono>

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
#include "terrain.h"

static const uint32_t TERRAIN_PATCH_RES = 85;

Terrain::Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap)
{
	scale = mapscale;
	glm::vec2 min = { -5.f, -5.f };
	glm::vec2 max = { mapscale.x + 5.f, mapscale.z + 5.f };
	patches = new Mesh { TERRAIN_PATCH_RES, min, max };

	normalmap = new Image { heightmap->width, heightmap->height, COLORSPACE_RGB };

	relief = new Texture { heightmap };
	// special wrapping mode so edges of the map are at height 0
	relief->change_wrapping(GL_CLAMP_TO_EDGE);

	normals = new Texture { normalmap };
	normals->change_wrapping(GL_CLAMP_TO_EDGE);

	land.compile("shaders/terrain.vert", GL_VERTEX_SHADER);
	land.compile("shaders/terrain.tesc", GL_TESS_CONTROL_SHADER);
	land.compile("shaders/terrain.tese", GL_TESS_EVALUATION_SHADER);
	land.compile("shaders/terrain.frag", GL_FRAGMENT_SHADER);
	land.link();
}

Terrain::~Terrain(void)
{
	delete patches;

	delete normalmap;
	delete normals;
	
	delete relief;
}

void Terrain::reload(const FloatImage *heightmap)
{
	relief->reload(heightmap);

	auto start = std::chrono::steady_clock::now();
	normalmap->create_normalmap(heightmap, 32.f);
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	std::cout << "normalmap elapsed time: " << elapsed_seconds.count() << "s\n";
	normals->reload(normalmap);
}

void Terrain::display(const Camera *camera)
{
	land.use();
	land.uniform_mat4("VP", camera->VP);
	land.uniform_vec3("CAM_POS", camera->position);
	land.uniform_vec3("MAP_SCALE", scale);

	relief->bind(GL_TEXTURE0);
	normals->bind(GL_TEXTURE1);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
