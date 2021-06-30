#include <vector>
#include <string>
#include <map>
#include <memory>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geom.h"
#include "../util/entity.h"
#include "../util/camera.h"
#include "../util/image.h"
#include "shader.h"
#include "texture.h"
#include "mesh.h"
#include "worldmap.h"

namespace gfx {

static const uint32_t WORLDMAP_PATCH_RES = 85;

Worldmap::Worldmap(const glm::vec3 &mapscale, const UTIL::Image<float> *heightmap, const UTIL::Image<uint8_t> *watermap, const UTIL::Image<uint8_t> *rainmap, const UTIL::Image<uint8_t> *materialmasks, const UTIL::Image<uint8_t> *factionsmap)
{
	scale = mapscale;
	faction_factor = 0.f;
	glm::vec2 min = { -5.f, -5.f };
	glm::vec2 max = { mapscale.x + 5.f, mapscale.z + 5.f };
	patches = std::make_unique<Mesh>(WORLDMAP_PATCH_RES, min, max);

	topology = std::make_unique<Texture>(heightmap);
	// special wrapping mode so edges of the map are at height 0
	topology->change_wrapping(GL_CLAMP_TO_EDGE);

	nautical = std::make_unique<Texture>(watermap);
	// special wrapping mode so edges of the map are at height 0
	nautical->change_wrapping(GL_CLAMP_TO_EDGE);

	rain = std::make_unique<Texture>(rainmap);
	rain->change_wrapping(GL_CLAMP_TO_EDGE);

	m_temperature = std::make_unique<Texture>(rainmap);
	m_temperature->change_wrapping(GL_CLAMP_TO_EDGE);

	normalmap.resize(heightmap->width(), heightmap->height(), UTIL::COLORSPACE_RGB);
	normals = std::make_unique<Texture>(&normalmap);
	normals->change_wrapping(GL_CLAMP_TO_EDGE);

	masks = std::make_unique<Texture>(materialmasks);
	masks->change_wrapping(GL_CLAMP_TO_EDGE);

	factions = std::make_unique<Texture>(factionsmap);
	factions->change_wrapping(GL_CLAMP_TO_EDGE);

	add_material("DISPLACEMENT", topology.get());
	add_material("NAUTICAL_DISPLACEMENT", nautical.get());
	add_material("NORMALMAP", normals.get());
	add_material("RAINMAP", rain.get());
	add_material("TEMPERATUREMAP", m_temperature.get());
	add_material("MASKMAP", masks.get());
	add_material("FACTIONSMAP", factions.get());

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
	
void Worldmap::add_material(const std::string &name, const Texture *texture)
{
	struct texture_binding_t texture_binding = {
		name,
		texture
	};

	materials.push_back(texture_binding);
}

void Worldmap::reload(const UTIL::Image<float> *heightmap, const UTIL::Image<uint8_t> *watermap, const UTIL::Image<uint8_t> *rainmap, const UTIL::Image<uint8_t> *materialmasks, const UTIL::Image<uint8_t> *factionsmap)
{
	topology->reload(heightmap);
	nautical->reload(watermap);
	rain->reload(rainmap);

	normalmap.create_normalmap(heightmap, 32.f);
	normals->reload(&normalmap);

	masks->reload(materialmasks);

	factions->reload(factionsmap);
}
	
void Worldmap::reload_temperature(const UTIL::Image<uint8_t> *temperature)
{
	m_temperature->reload(temperature);
}

void Worldmap::reload_factionsmap(const UTIL::Image<uint8_t> *factionsmap)
{
	factions->reload(factionsmap);
}
	
void Worldmap::reload_masks(const UTIL::Image<uint8_t> *mask_image)
{
	masks->reload(mask_image);
}

void Worldmap::change_atmosphere(const glm::vec3 &fogclr, float fogfctr, const glm::vec3 &sunposition)
{
	fogcolor = fogclr;
	fogfactor = fogfctr;
	sunpos = sunposition;
}

void Worldmap::display_land(const UTIL::Camera *camera) const
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

	for (int i = 0; i < materials.size(); i++) {
		const auto &binding = materials[i];
		land.uniform_int(binding.name.c_str(), i);
		binding.texture->bind(GL_TEXTURE0 + i);
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Worldmap::display_water(const UTIL::Camera *camera, float time) const
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

	for (int i = 0; i < materials.size(); i++) {
		const auto &binding = materials[i];
		water.uniform_int(binding.name.c_str(), i);
		binding.texture->bind(GL_TEXTURE0 + i);
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
	
void Worldmap::change_groundcolors(const glm::vec3 &dry, const glm::vec3 &lush, const glm::vec3 &base_rock_min, const glm::vec3 &base_rock_max, const glm::vec3 desert_rock_min, const glm::vec3 desert_rock_max)
{
	grass_dry = dry;
	grass_lush = lush;

	land.use();
	land.uniform_vec3("ROCK_BASE_MIN", base_rock_min);
	land.uniform_vec3("ROCK_BASE_MAX", base_rock_max);
	land.uniform_vec3("ROCK_DESERT_MIN", desert_rock_min);
	land.uniform_vec3("ROCK_DESERT_MAX", desert_rock_max);
}
	
void Worldmap::set_faction_factor(float factor)
{
	faction_factor = glm::clamp(factor, 0.f, 1.f);
}

};
