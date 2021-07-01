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

Worldmap::Worldmap(const glm::vec3 &mapscale, const util::Image<float> *heightmap, const util::Image<uint8_t> *watermap, const util::Image<uint8_t> *rainmap, const util::Image<uint8_t> *materialmasks, const util::Image<uint8_t> *factionsmap)
{
	m_scale = mapscale;
	faction_factor = 0.f;
	glm::vec2 min = { -5.f, -5.f };
	glm::vec2 max = { mapscale.x + 5.f, mapscale.z + 5.f };
	m_mesh = std::make_unique<Mesh>(WORLDMAP_PATCH_RES, min, max);

	m_textures.topology = std::make_unique<Texture>(heightmap);
	// special wrapping mode so edges of the map are at height 0
	m_textures.topology->change_wrapping(GL_CLAMP_TO_EDGE);

	m_textures.nautical = std::make_unique<Texture>(watermap);
	// special wrapping mode so edges of the map are at height 0
	m_textures.nautical->change_wrapping(GL_CLAMP_TO_EDGE);

	m_textures.rain = std::make_unique<Texture>(rainmap);
	m_textures.rain->change_wrapping(GL_CLAMP_TO_EDGE);

	m_textures.temperature = std::make_unique<Texture>(rainmap);
	m_textures.temperature->change_wrapping(GL_CLAMP_TO_EDGE);

	m_normalmap.resize(heightmap->width(), heightmap->height(), util::COLORSPACE_RGB);
	m_textures.normals = std::make_unique<Texture>(&m_normalmap);
	m_textures.normals->change_wrapping(GL_CLAMP_TO_EDGE);

	m_textures.masks = std::make_unique<Texture>(materialmasks);
	m_textures.masks->change_wrapping(GL_CLAMP_TO_EDGE);

	m_textures.factions = std::make_unique<Texture>(factionsmap);
	m_textures.factions->change_wrapping(GL_CLAMP_TO_EDGE);

	add_material("DISPLACEMENT", m_textures.topology.get());
	add_material("NAUTICAL_DISPLACEMENT", m_textures.nautical.get());
	add_material("NORMALMAP", m_textures.normals.get());
	add_material("RAINMAP", m_textures.rain.get());
	add_material("TEMPERATUREMAP", m_textures.temperature.get());
	add_material("MASKMAP", m_textures.masks.get());
	add_material("FACTIONSMAP", m_textures.factions.get());

	m_land.compile("shaders/campaign/worldmap.vert", GL_VERTEX_SHADER);
	m_land.compile("shaders/campaign/worldmap.tesc", GL_TESS_CONTROL_SHADER);
	m_land.compile("shaders/campaign/worldmap.tese", GL_TESS_EVALUATION_SHADER);
	m_land.compile("shaders/campaign/worldmap.frag", GL_FRAGMENT_SHADER);
	m_land.link();

	m_water.compile("shaders/campaign/ocean.vert", GL_VERTEX_SHADER);
	m_water.compile("shaders/campaign/ocean.tesc", GL_TESS_CONTROL_SHADER);
	m_water.compile("shaders/campaign/ocean.tese", GL_TESS_EVALUATION_SHADER);
	m_water.compile("shaders/campaign/ocean.frag", GL_FRAGMENT_SHADER);
	m_water.link();
}
	
void Worldmap::add_material(const std::string &name, const Texture *texture)
{
	texture_binding_t texture_binding = {
		name,
		texture
	};

	m_materials.push_back(texture_binding);
}

void Worldmap::reload(const util::Image<float> *heightmap, const util::Image<uint8_t> *watermap, const util::Image<uint8_t> *rainmap, const util::Image<uint8_t> *materialmasks, const util::Image<uint8_t> *factionsmap)
{
	m_textures.topology->reload(heightmap);
	m_textures.nautical->reload(watermap);
	m_textures.rain->reload(rainmap);

	m_normalmap.create_normalmap(heightmap, 32.f);
	m_textures.normals->reload(&m_normalmap);

	m_textures.masks->reload(materialmasks);

	m_textures.factions->reload(factionsmap);
}
	
void Worldmap::reload_temperature(const util::Image<uint8_t> *temperature)
{
	m_textures.temperature->reload(temperature);
}

void Worldmap::reload_factionsmap(const util::Image<uint8_t> *factionsmap)
{
	m_textures.factions->reload(factionsmap);
}
	
void Worldmap::reload_masks(const util::Image<uint8_t> *mask_image)
{
	m_textures.masks->reload(mask_image);
}

void Worldmap::change_atmosphere(const glm::vec3 &fogclr, float fogfctr, const glm::vec3 &sunposition)
{
	fogcolor = fogclr;
	fogfactor = fogfctr;
	sunpos = sunposition;
}

void Worldmap::display_land(const util::Camera *camera) const
{
	m_land.use();
	m_land.uniform_mat4("VP", camera->VP);
	m_land.uniform_vec3("CAM_POS", camera->position);
	m_land.uniform_vec3("MAP_SCALE", m_scale);
	m_land.uniform_vec3("FOG_COLOR", fogcolor);
	m_land.uniform_float("FOG_FACTOR", fogfactor);
	m_land.uniform_vec3("GRASS_DRY", grass_dry);
	m_land.uniform_vec3("GRASS_LUSH", grass_lush);
	m_land.uniform_float("FACTION_FACTOR", faction_factor);

	for (int i = 0; i < m_materials.size(); i++) {
		const auto &binding = m_materials[i];
		m_land.uniform_int(binding.name.c_str(), i);
		binding.texture->bind(GL_TEXTURE0 + i);
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	m_mesh->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Worldmap::display_water(const util::Camera *camera, float time) const
{
	m_water.use();
	m_water.uniform_mat4("VP", camera->VP);
	m_water.uniform_vec3("CAM_POS", camera->position);
	m_water.uniform_float("NEAR_CLIP", camera->nearclip);
	m_water.uniform_float("FAR_CLIP", camera->farclip);
	m_water.uniform_float("TIME", time);
	m_water.uniform_vec3("MAP_SCALE", m_scale);
	m_water.uniform_vec3("FOG_COLOR", fogcolor);
	m_water.uniform_float("FOG_FACTOR", fogfactor);
	m_water.uniform_vec3("SUN_POS", sunpos);

	for (int i = 0; i < m_materials.size(); i++) {
		const auto &binding = m_materials[i];
		m_water.uniform_int(binding.name.c_str(), i);
		binding.texture->bind(GL_TEXTURE0 + i);
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	m_mesh->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
	
void Worldmap::change_groundcolors(const glm::vec3 &dry, const glm::vec3 &lush, const glm::vec3 &base_rock_min, const glm::vec3 &base_rock_max, const glm::vec3 desert_rock_min, const glm::vec3 desert_rock_max)
{
	grass_dry = dry;
	grass_lush = lush;

	m_land.use();
	m_land.uniform_vec3("ROCK_BASE_MIN", base_rock_min);
	m_land.uniform_vec3("ROCK_BASE_MAX", base_rock_max);
	m_land.uniform_vec3("ROCK_DESERT_MIN", desert_rock_min);
	m_land.uniform_vec3("ROCK_DESERT_MAX", desert_rock_max);
}
	
void Worldmap::set_faction_factor(float factor)
{
	faction_factor = glm::clamp(factor, 0.f, 1.f);
}

};
