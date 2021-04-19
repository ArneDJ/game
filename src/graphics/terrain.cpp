#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <random>

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
#include "../core/model.h"
#include "shadow.h"
#include "terrain.h"

static const uint32_t TERRAIN_PATCH_RES = 85;

Terrain::Terrain(const glm::vec3 &mapscale, const FloatImage *heightmap, const Image *normalmap, const GLTF::Model *grassmodel)
{
	scale = mapscale;
	glm::vec2 min = { -5.f, -5.f };
	glm::vec2 max = { mapscale.x + 5.f, mapscale.z + 5.f };
	patches = new Mesh { TERRAIN_PATCH_RES, min, max };

	relief = new Texture { heightmap };
	// special wrapping mode so edges of the map are at height 0
	relief->change_wrapping(GL_CLAMP_TO_EDGE);

	normals = new Texture { normalmap };
	normals->change_wrapping(GL_CLAMP_TO_EDGE);

	land.compile("shaders/battle/terrain.vert", GL_VERTEX_SHADER);
	land.compile("shaders/battle/terrain.tesc", GL_TESS_CONTROL_SHADER);
	land.compile("shaders/battle/terrain.tese", GL_TESS_EVALUATION_SHADER);
	land.compile("shaders/battle/terrain.frag", GL_FRAGMENT_SHADER);
	land.link();

	water.compile("shaders/battle/water.vert", GL_VERTEX_SHADER);
	water.compile("shaders/battle/water.tesc", GL_TESS_CONTROL_SHADER);
	water.compile("shaders/battle/water.tese", GL_TESS_EVALUATION_SHADER);
	water.compile("shaders/battle/water.frag", GL_FRAGMENT_SHADER);
	water.link();

	grass = new Grass { grassmodel };
}

void Terrain::load_materials(const std::vector<const Texture*> textures)
{
	materials.clear();
	materials.insert(materials.begin(), textures.begin(), textures.end());
}

Terrain::~Terrain(void)
{
	materials.clear();

	delete grass;

	delete patches;

	delete normals;
	
	delete relief;
}
	
void Terrain::change_atmosphere(const glm::vec3 &sun, const glm::vec3 &fogclr, float fogfctr)
{
	fogcolor = fogclr;
	fogfactor = fogfctr;
	sunpos = sun;
	grass->fogcolor = fogclr;
	grass->fogfactor = fogfctr;
}

void Terrain::change_grass(const glm::vec3 &color)
{
	grasscolor = color;
	grass->color = color;
}

void Terrain::reload(const FloatImage *heightmap, const Image *normalmap)
{
	relief->reload(heightmap);

	normals->reload(normalmap);
	
	grass->spawn(scale, heightmap, normalmap);
}
	
void Terrain::update_shadow(const Shadow *shadow, bool show_cascades)
{
	land.use();
	land.uniform_bool("SHOW_CASCADES", show_cascades);
	land.uniform_vec4("SPLIT", shadow->get_splitdepth());
	land.uniform_mat4_array("SHADOWSPACE", shadow->get_biased_shadowspaces());
		
	shadow->bind_textures(GL_TEXTURE10);
}

void Terrain::display_land(const Camera *camera) const
{
	land.use();
	land.uniform_mat4("VP", camera->VP);
	land.uniform_vec3("CAM_POS", camera->position);
	land.uniform_vec3("MAP_SCALE", scale);
	land.uniform_vec3("SUN_POS", sunpos);
	land.uniform_vec3("FOG_COLOR", fogcolor);
	land.uniform_float("FOG_FACTOR", fogfactor);
	land.uniform_vec3("GRASS_COLOR", grasscolor);

	relief->bind(GL_TEXTURE0);
	normals->bind(GL_TEXTURE1);

	for (int i = 0; i < materials.size(); i++) {
		materials[i]->bind(GL_TEXTURE2 + i);
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	//
}

void Terrain::display_water(const Camera *camera, float time) const
{
	water.use();
	water.uniform_mat4("VP", camera->VP);
	water.uniform_vec3("CAM_POS", camera->position);
	water.uniform_float("NEAR_CLIP", camera->nearclip);
	water.uniform_float("FAR_CLIP", camera->farclip);
	water.uniform_vec3("MAP_SCALE", scale);
	water.uniform_vec3("SUN_POS", sunpos);
	water.uniform_float("TIME", time);
	water.uniform_vec2("WIND_DIR", glm::vec2(0.5, 0.5));
	water.uniform_vec3("FOG_COLOR", fogcolor);
	water.uniform_float("FOG_FACTOR", fogfactor);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	patches->draw();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
	
void Terrain::display_grass(const Camera *camera) const
{
	grass->display(camera);
}

Grass::Grass(const GLTF::Model *mod)
{
	model = mod;

	tbuffer.matrices.resize(20*20*1000);
	tbuffer.alloc(GL_DYNAMIC_DRAW);

	shader.compile("shaders/battle/grass.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/battle/grass.frag", GL_FRAGMENT_SHADER);
	shader.link();
}

Grass::~Grass(void)
{
	//delete mesh;
}

void Grass::spawn(const glm::vec3 &scale, const FloatImage *heightmap, const Image *normalmap)
{
	glm::vec2 hmapscale = {
		float(heightmap->width) / scale.x,
		float(heightmap->height) / scale.z
	};

	// random points
	glm::vec2 offset = { 3000.f, 3000.f };

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);

	int index = 0;
	for (int i = 0; i < 20; i++) {
		for (int j = 0; j < 20; j++) {
			glm::vec2 min = { offset.x, offset.y };
			glm::vec2 max = { offset.x + 50.f, offset.y + 50.f };
			std::uniform_real_distribution<float> map_x(min.x, max.x);
			std::uniform_real_distribution<float> map_y(-0.25f, 0.25f);
			std::uniform_real_distribution<float> map_z(min.y, max.y);
			for (int i = 0; i < 1000; i++) {
				glm::vec3 position = { map_x(gen), 0.f, map_z(gen) };
				position.y = scale.y * heightmap->sample(hmapscale.x*position.x, hmapscale.y*position.z, CHANNEL_RED);
				position.y += map_y(gen) + 0.5f;
				glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));

				glm::mat4 T = glm::translate(glm::mat4(1.f), position);
				glm::mat4 R = glm::mat4(rotation);
				glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(2.f, 2.f, 2.f));
				tbuffer.matrices[index++] = T * R * S;
			}

			offset.y += 50.f;
		}
			
		offset.x += 50.f;
		offset.y = 3000.f;
	}

	tbuffer.update();
}

void Grass::display(const Camera *camera) const
{
	glDisable(GL_CULL_FACE);

	shader.use();
	shader.uniform_mat4("VP", camera->VP);
	shader.uniform_vec3("CAM_POS", camera->position);

	glm::mat4 MVP = camera->VP;
	shader.uniform_mat4("MVP", MVP);
	shader.uniform_mat4("MODEL", glm::mat4(1.f));
	shader.uniform_vec3("COLOR", color);
	shader.uniform_vec3("FOG_COLOR", fogcolor);
	shader.uniform_float("FOG_FACTOR", fogfactor);
	tbuffer.bind(GL_TEXTURE10);
	model->display_instanced(20*20*1000);
	
	glEnable(GL_CULL_FACE);
}
