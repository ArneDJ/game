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

#include "../core/geom.h"
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

Terrain::~Terrain(void)
{
	materials.clear();

	delete grass;

	delete patches;

	delete normals;
	
	delete relief;
}
	
void Terrain::load_materials(const std::vector<const Texture*> textures)
{
	materials.clear();
	materials.insert(materials.begin(), textures.begin(), textures.end());
}

void Terrain::prepare(void)
{
	grass->generate();
}

void Terrain::change_atmosphere(const glm::vec3 &sun, const glm::vec3 &fogclr, float fogfctr)
{
	fogcolor = fogclr;
	fogfactor = fogfctr;
	sunpos = sun;
}

void Terrain::change_grass(const glm::vec3 &color)
{
	grasscolor = color;
	grass->colorize(color, fogcolor, sunpos, fogfactor);
}

void Terrain::reload(const FloatImage *heightmap, const Image *normalmap)
{
	relief->reload(heightmap);

	normals->reload(normalmap);
	
	grass->place(scale, heightmap, normalmap);
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
	relief->bind(GL_TEXTURE1);
	normals->bind(GL_TEXTURE2);
	grass->display(camera, scale);
}

GrassBox::GrassBox(const GLTF::Model *mod, const struct rectangle &bounds)
{
	boundaries = bounds;
	model = mod;

	int nsprites = 100;
	transforms.resize(nsprites);
	tbuffer.matrices.resize(nsprites);
	tbuffer.alloc(GL_STATIC_DRAW);

	glm::vec2 mid = segment_midpoint(bounds.min, bounds.max);
	box.c = glm::vec3(mid.x, 0.f, mid.y);
	glm::vec2 dist = { 0.5f * (bounds.max.x - bounds.min.x), 0.5f * (bounds.max.y - bounds.min.y) };
	box.r = glm::vec3(dist.x, 1.f, dist.y);
}

void GrassBox::generate(void)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
	std::uniform_real_distribution<float> scale_dist(1.f, 2.f);

	glm::vec2 min = boundaries.min;
	glm::vec2 max = boundaries.max;
	std::uniform_real_distribution<float> map_x(min.x, max.x);
	std::uniform_real_distribution<float> map_y(-0.05f, 0.05f);
	std::uniform_real_distribution<float> map_z(min.y, max.y);
	for (int i = 0; i < transforms.size(); i++) {
		glm::vec3 position = { map_x(gen), 0.f, map_z(gen) };
		glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));

		glm::mat4 T = glm::translate(glm::mat4(1.f), position);
		glm::mat4 R = glm::mat4(rotation);
		float scale = scale_dist(gen);
		glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(scale, scale, scale));
		transforms[i++] = T * R * S;
	}
}

void GrassBox::place(const glm::vec3 &scale, const FloatImage *heightmap, const Image *normalmap)
{
	glm::vec2 hmapscale = {
		float(heightmap->width) / scale.x,
		float(heightmap->height) / scale.z
	};

	float min = std::numeric_limits<float>::max();
	float max = std::numeric_limits<float>::min();
	instance_count = 0;
	for (int i = 0; i < transforms.size(); i++) {
		glm::vec4 position = transforms[i][3];
		float slope = 1.f - (normalmap->sample(hmapscale.x*position.x, hmapscale.y*position.z, CHANNEL_GREEN) / 255.f);
		if (slope > 0.15f) {
			continue;
		}
		position.y = scale.y * heightmap->sample(hmapscale.x*position.x, hmapscale.y*position.z, CHANNEL_RED);
		if (position.y < min) { min = position.y; }
		if (position.y > max) { max = position.y; }
		tbuffer.matrices[instance_count++] = transforms[i];
	}

	box.r.y = 0.5f * (max - min);
	box.c.y = box.r.y + min;

	tbuffer.update();
}

void GrassBox::display(void) const
{
	tbuffer.bind(GL_TEXTURE10);
	model->display_instanced(instance_count);
}

Grass::Grass(const GLTF::Model *mod)
{
	uint16_t width = 64;
	uint16_t height = 64;
	glm::vec2 min = { 2560.f, 2560.f };
	glm::vec2 max = { 3584.f, 3584.f };
	glm::vec2 spacing = { (max.x - min.x) / float(width), (max.y - min.y) / float(height) };
	glm::vec2 offset = min;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			struct rectangle rect = { offset, offset + spacing };
			GrassBox *grassbox = new GrassBox { mod, rect };
			grassboxes.push_back(grassbox);
			offset.y += spacing.y;
		}
		offset.x += spacing.x;
		offset.y = min.y;
	}

	shader.compile("shaders/battle/grass.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/battle/grass.frag", GL_FRAGMENT_SHADER);
	shader.link();
}

Grass::~Grass(void)
{
	for (int i = 0; i < grassboxes.size(); i++) {
		delete grassboxes[i];
	}
}
	
void Grass::colorize(const glm::vec3 &colr, const glm::vec3 &fogclr, const glm::vec3 &sun, float fogfctr)
{
	color = colr;
	fogcolor = fogclr;
	sunpos = sun;
	fogfactor = fogfctr;
}

void Grass::generate(void)
{
	for (auto &grassbox : grassboxes) {
		grassbox->generate();
	}
}

void Grass::place(const glm::vec3 &scale, const FloatImage *heightmap, const Image *normalmap)
{
	for (auto &grassbox : grassboxes) {
		grassbox->place(scale, heightmap, normalmap);
	}
}

void Grass::display(const Camera *camera, const glm::vec3 &scale) const
{
	glDisable(GL_CULL_FACE);

	shader.use();
	shader.uniform_mat4("VP", camera->VP);
	shader.uniform_vec3("CAM_POS", camera->position);

	glm::mat4 MVP = camera->VP;
	shader.uniform_mat4("MVP", MVP);
	shader.uniform_mat4("MODEL", glm::mat4(1.f));
	shader.uniform_vec3("MAPSCALE", scale);
	shader.uniform_vec3("COLOR", color);
	shader.uniform_vec3("SUN_POS", sunpos);
	shader.uniform_vec3("FOG_COLOR", fogcolor);
	shader.uniform_float("FOG_FACTOR", fogfactor);

	glm::mat4 projection = glm::perspective(glm::radians(camera->FOV), camera->aspectratio, camera->nearclip, 200.f);
	// frustum culling first attempt
	glm::vec4 planes[6];
	frustum_to_planes(projection * camera->viewing, planes);
	for (const auto &grassbox : grassboxes) {
		const struct AABB *bbox = grassbox->boundbox();
		glm::vec3 min = bbox->c - bbox->r;
		glm::vec3 max = bbox->c + bbox->r;
		if (AABB_in_frustum(min, max, planes)) {
			grassbox->display();
		}
	}
	
	glEnable(GL_CULL_FACE);
}
