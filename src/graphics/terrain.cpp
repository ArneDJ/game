#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <chrono>

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

	//grass = new Grass { grassmodel };
	auto start = std::chrono::steady_clock::now();
	grass = new GrassSystem { grassmodel };
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	std::cout << "grass elapsed time: " << elapsed_seconds.count() << "s\n";
	
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
	
	auto start = std::chrono::steady_clock::now();
	grass->refresh(heightmap, scale);
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	std::cout << "grass refresh time: " << elapsed_seconds.count() << "s\n";
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

GrassRoots::GrassRoots(const GLTF::Model *mod, uint32_t count)
{
	model = mod;

	// generate random points
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
	std::uniform_real_distribution<float> scale_dist(1.f, 2.f);

	std::uniform_real_distribution<float> map_x(0.f, 1.f);
	std::uniform_real_distribution<float> map_z(0.f, 1.f);

	for (int i = 0; i < count; i++) {
		glm::vec3 position = { map_x(gen), 0.f, map_z(gen) };
		glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));

		glm::mat4 T = glm::translate(glm::mat4(1.f), position);
		glm::mat4 R = glm::mat4(rotation);
		float scale = scale_dist(gen);
		glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(scale, scale, scale));
		tbuffer.matrices.push_back(T * R * S);
	}
	
	tbuffer.alloc(GL_STATIC_DRAW);
	tbuffer.update();
}

void GrassRoots::display(void) const
{
	tbuffer.bind(GL_TEXTURE10);
	model->display_instanced(tbuffer.matrices.size());
}
	
GrassChunk::GrassChunk(const GrassRoots *grassroots, const glm::vec3 &min, const glm::vec3 &max)
{
	roots = grassroots;

	bbox.min = min;
	bbox.max = max;

	offset = translate_3D_to_2D(min);
	scale = translate_3D_to_2D(max) - translate_3D_to_2D(min);
}

GrassSystem::GrassSystem(const GLTF::Model *mod)
{
	// generate grass roots
	int root_res = 8;
	for (int i = 0; i < root_res; i++) {
		for (int j = 0; j < root_res; j++) {
			GrassRoots *root = new GrassRoots { mod, 200 };
			roots.push_back(root);
		}
	}

	// create grass chunks
	uint16_t width = 64;
	uint16_t height = 64;
	glm::vec2 min = { 2048.f, 2048.f };
	glm::vec2 max = { 4096.f, 4096.f };
	glm::vec2 spacing = { (max.x - min.x) / float(width), (max.y - min.y) / float(height) };
	glm::vec2 offset = min;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			// default bounding box
			glm::vec3 min = { offset.x, 0.f, offset.y };
			glm::vec3 max = { offset.x + spacing.x, 0.f, offset.y + spacing.y };
			// find tiling grass roots
			int x_coord = x % root_res;
			int y_coord = y % root_res;
			int index = y_coord * root_res + x_coord;
			if (index >= 0 && index < roots.size()) {
				GrassChunk *chunk = new GrassChunk { roots[index], min, max };
				chunks.push_back(chunk);	
			}
			offset.y += spacing.y;
		}
		offset.x += spacing.x;
		offset.y = min.y;
	}

	printf("total grass chunks %d\n", chunks.size());

	shader.compile("shaders/battle/grass.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/battle/grass.frag", GL_FRAGMENT_SHADER);
	shader.link();
}

GrassSystem::~GrassSystem(void)
{
	for (int i = 0; i < chunks.size(); i++) {
		delete chunks[i];
	}
	chunks.clear();

	for (int i = 0; i < roots.size(); i++) {
		delete roots[i];
	}
	roots.clear();
}

void GrassSystem::refresh(const FloatImage *heightmap, const glm::vec3 &scale)
{
	// update bounding boxes based on new heightmap
	for (auto &chunk : chunks) {
		// find lowest and highest pixel in terrain heightmap to create bounding box
		float min_height = std::numeric_limits<float>::max();
		float max_height = std::numeric_limits<float>::min();

		glm::vec2 min = translate_3D_to_2D(chunk->bbox.min) / translate_3D_to_2D(scale);
		glm::vec2 max = translate_3D_to_2D(chunk->bbox.max) / translate_3D_to_2D(scale);
		
		int rect_min_x = floorf(min.x * heightmap->width);
		int rect_min_y = floorf(min.y * heightmap->height);
		int rect_max_x = floorf(max.x * heightmap->width);
		int rect_max_y = floorf(max.y * heightmap->height);

		for (int i = rect_min_x; i < rect_max_x; i++) {
			for (int j = rect_min_y; j < rect_max_y; j++) {
				float height = heightmap->sample(i, j, CHANNEL_RED);
				if (height < min_height) { min_height = height; }
				if (height > max_height) { max_height = height; }
			}
		}

		if (min_height > 0.f && min_height < scale.y)  {
			chunk->bbox.min.y = scale.y * min_height;
		}
		if (max_height > 0.f && max_height < scale.y) {
			chunk->bbox.max.y = scale.y * max_height;
		}
	}
}

void GrassSystem::colorize(const glm::vec3 &colr, const glm::vec3 &fogclr, const glm::vec3 &sun, float fogfctr)
{
	color = colr;
	fogcolor = fogclr;
	sunpos = sun;
	fogfactor = fogfctr;
}

void GrassSystem::display(const Camera *camera, const glm::vec3 &scale) const
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

	for (const auto &chunk : chunks) {
		if (AABB_in_frustum(chunk->bbox.min, chunk->bbox.max, planes)) {
			shader.uniform_vec2("ROOT_OFFSET", chunk->offset);
			shader.uniform_vec2("CHUNK_SCALE", chunk->scale);
			chunk->roots->display();
		}
	}
	
	glEnable(GL_CULL_FACE);
}
