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

#include "../core/entity.h"
#include "../core/shader.h"
#include "../core/camera.h"
#include "../core/image.h"
#include "../core/texture.h"
#include "../core/mesh.h"
#include "worldmap.h"

static const uint32_t WORLDMAP_PATCH_RES = 85;

Worldmap::Worldmap(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *watermap, const Image *rainmap, const Image *materialmasks, const Image *factionsmap)
{
	scale = mapscale;
	faction_factor = 0.f;
	glm::vec2 min = { -5.f, -5.f };
	glm::vec2 max = { mapscale.x + 5.f, mapscale.z + 5.f };
	patches = new Mesh { WORLDMAP_PATCH_RES, min, max };

	topology = new Texture { heightmap };
	// special wrapping mode so edges of the map are at height 0
	topology->change_wrapping(GL_CLAMP_TO_EDGE);

	nautical = new Texture { watermap };
	// special wrapping mode so edges of the map are at height 0
	nautical->change_wrapping(GL_CLAMP_TO_EDGE);

	rain = new Texture { rainmap };
	rain->change_wrapping(GL_CLAMP_TO_EDGE);

	normalmap = new FloatImage { heightmap->width, heightmap->height, COLORSPACE_RGB };
	normals = new Texture { normalmap };
	normals->change_wrapping(GL_CLAMP_TO_EDGE);

	masks = new Texture { materialmasks };
	masks->change_wrapping(GL_CLAMP_TO_EDGE);

	factions = new Texture { factionsmap };
	factions->change_wrapping(GL_CLAMP_TO_EDGE);

	land.compile("shaders/campaign/worldmap.vert", GL_VERTEX_SHADER);
	land.compile("shaders/campaign/worldmap.tesc", GL_TESS_CONTROL_SHADER);
	land.compile("shaders/campaign/worldmap.tese", GL_TESS_EVALUATION_SHADER);
	land.compile("shaders/campaign/worldmap.frag", GL_FRAGMENT_SHADER);
	land.link();

	water.compile("shaders/campaign/ocean.vert", GL_VERTEX_SHADER);
	water.compile("shaders/campaign/ocean.tesc", GL_TESS_CONTROL_SHADER);
	water.compile("shaders/campaign/ocean.tese", GL_TESS_EVALUATION_SHADER);
	water.compile("shaders/campaign/ocean.frag", GL_FRAGMENT_SHADER);
	water.link();
}
	
void Worldmap::load_materials(const std::vector<const Texture*> textures)
{
	materials.clear();
	materials.insert(materials.begin(), textures.begin(), textures.end());
}

Worldmap::~Worldmap(void)
{
	materials.clear();

	delete patches;
	
	delete topology;
	delete rain;

	delete normals;
	delete normalmap;

	delete masks;

	delete factions;
}

void Worldmap::reload(const FloatImage *heightmap, const Image *watermap, const Image *rainmap, const Image *materialmasks, const Image *factionsmap)
{
	topology->reload(heightmap);
	nautical->reload(watermap);
	rain->reload(rainmap);

	normalmap->create_normalmap(heightmap, 32.f);
	normals->reload(normalmap);

	masks->reload(materialmasks);

	factions->reload(factionsmap);
}

void Worldmap::change_atmosphere(const glm::vec3 &fogclr, float fogfctr, const glm::vec3 &sunposition)
{
	fogcolor = fogclr;
	fogfactor = fogfctr;
	sunpos = sunposition;
}

void Worldmap::display_land(const Camera *camera) const
{
	land.use();
	land.uniform_mat4("VP", camera->VP);
	land.uniform_vec3("CAM_POS", camera->position);
	land.uniform_vec3("MAP_SCALE", scale);
	land.uniform_vec3("FOG_COLOR", fogcolor);
	land.uniform_float("FOG_FACTOR", fogfactor);
	land.uniform_vec3("GRASS_DRY", grass_dry);
	land.uniform_vec3("GRASS_LUSH", grass_lush);
	land.uniform_float("FACTION_FACTOR", faction_factor);

	topology->bind(GL_TEXTURE0);
	normals->bind(GL_TEXTURE1);
	rain->bind(GL_TEXTURE2);
	masks->bind(GL_TEXTURE3);
	factions->bind(GL_TEXTURE4);

	for (int i = 0; i < materials.size(); i++) {
		materials[i]->bind(GL_TEXTURE5 + i);
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Worldmap::display_water(const Camera *camera, float time) const
{
	water.use();
	water.uniform_mat4("VP", camera->VP);
	water.uniform_vec3("CAM_POS", camera->position);
	water.uniform_float("NEAR_CLIP", camera->nearclip);
	water.uniform_float("FAR_CLIP", camera->farclip);
	water.uniform_float("TIME", time);
	water.uniform_vec3("MAP_SCALE", scale);
	water.uniform_vec3("FOG_COLOR", fogcolor);
	water.uniform_float("FOG_FACTOR", fogfactor);
	water.uniform_vec3("SUN_POS", sunpos);

	nautical->bind(GL_TEXTURE0);

	for (int i = 0; i < materials.size(); i++) {
		materials[i]->bind(GL_TEXTURE4 + i);
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
	
void Worldmap::change_groundcolors(const glm::vec3 &dry, const glm::vec3 &lush)
{
	grass_dry = dry;
	grass_lush = lush;
}
	
void Worldmap::set_faction_factor(float factor)
{
	faction_factor = glm::clamp(factor, 0.f, 1.f);
}
