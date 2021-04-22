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

#include "extern/poisson/PoissonGenerator.h"

#include "core/geom.h"
#include "core/entity.h"
#include "core/camera.h"
#include "core/shader.h"
#include "core/image.h"
#include "core/texture.h"
#include "core/mesh.h"
#include "core/model.h"

#include "tree.h"

static const int MAX_TREES = 40000;
static const uint16_t DENSITY_MAP_RES = 256;

TreeKind::TreeKind(const GLTF::Model *modl, const GLTF::Model *board)
{
	model = modl;
	billboard = board;
	tbuffer.alloc(GL_STATIC_DRAW);
	boardbuffer.alloc(GL_STATIC_DRAW);
}

TreeKind::~TreeKind(void)
{
	clear();
}
	
void TreeKind::clear(void)
{
	for (int i = 0; i < entities.size(); i++) {
		delete entities[i];
	}
	entities.clear();
	tbuffer.matrices.clear();
	boardbuffer.matrices.clear();
}
	
void TreeKind::add(const glm::vec3 &position, const glm::quat &rotation)
{
	Entity *ent = new Entity { position, rotation };
	entities.push_back(ent);
}
	
void TreeKind::prepare(void)
{
	// TODO avoid duplicates
	tbuffer.matrices.resize(entities.size());
	boardbuffer.matrices.resize(entities.size());
}

void TreeKind::display(uint32_t instancecount, uint32_t billboardcount)
{
	tbuffer.update();
	tbuffer.bind(GL_TEXTURE10);
	model->display_instanced(instancecount);

	boardbuffer.update();
	boardbuffer.bind(GL_TEXTURE10);
	billboard->display_instanced(billboardcount);
}

void TreeForest::init(const GLTF::Model *pinemodel, const GLTF::Model *pine_billboard)
{
	pine = new TreeKind { pinemodel, pine_billboard };
	density = new Image { DENSITY_MAP_RES, DENSITY_MAP_RES, COLORSPACE_GRAYSCALE };

	uint16_t width = 64;
	uint16_t height = 64;
	glm::vec2 min = { 0.f, 0.f };
	glm::vec2 max = { 6144.f, 6144.f };
	glm::vec2 spacing = { (max.x - min.x) / float(width), (max.y - min.y) / float(height) };
	glm::vec2 offset = min;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			struct rectangle rect = { offset, offset + spacing };
			//GrassBox *grassbox = new GrassBox { mod, rect };
			struct TreeBox box;
			// TODO proper box
			//glm::vec2 mid = segment_midpoint(rect.min, rect.max);
			//box.box.c = glm::vec3(mid.x, 0.f, mid.y);
			//glm::vec2 dist = { 0.5f * (rect.max.x - rect.min.x), 0.5f * (rect.max.y - rect.min.y) };
			//box.box.r = glm::vec3(dist.x, 1.f, dist.y);
			box.min = glm::vec3(rect.min.x, 0.f,  rect.min.y);
			box.max = glm::vec3(rect.max.x, 256.f,  rect.max.y);

			boxes.push_back(box);
			offset.y += spacing.y;
		}
		offset.x += spacing.x;
		offset.y = min.y;
	}

	shader.compile("shaders/tree.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/tree.frag", GL_FRAGMENT_SHADER);
	shader.link();
}

void TreeForest::teardown(void)
{
	delete pine;
	delete density;
}
	
void TreeForest::clear(void)
{
	pine->clear();
	for (int i = 0; i < boxes.size(); i++) {
		boxes[i].entities.clear();
	}
}
	
void TreeForest::spawn(const glm::vec3 &mapscale, const FloatImage *heightmap, uint8_t precipitation)
{
	// first clear the forest
	clear();

	// create density map
	FastNoise fastnoise;
	fastnoise.SetSeed(1337);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(0.01f);

	density->noise(&fastnoise, glm::vec2(1.f, 1.f), CHANNEL_RED);
	density->write("density.png");

	// spawn the forest
	glm::vec2 hmapscale = {
		float(heightmap->width) / mapscale.x,
		float(heightmap->height) / mapscale.z
	};

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
	std::uniform_real_distribution<float> scale_dist(1.f, 2.f);

	printf("precipitation %f\n", precipitation / 255.f);

	float rain = precipitation / 255.f;

	PoissonGenerator::DefaultPRNG PRNG;
	const auto points = PoissonGenerator::generatePoissonPoints(MAX_TREES, PRNG, true);
	for (const auto &point : points) {
		float P = density->sample(point.x * density->width, point.y * density->height, CHANNEL_RED) / 255.f;
		P *= rain * rain;
		float R = PRNG.randomFloat();
		if ( R > P ) { continue; }
		glm::vec3 position = { point.x * mapscale.x, 0.f, point.y * mapscale.z };
		position.y = mapscale.y * heightmap->sample(hmapscale.x*position.x, hmapscale.y*position.z, CHANNEL_RED);
		glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));
		pine->add(position, rotation);
	}

	// add entities to cull boxes
	glm::vec2 boxscale = {
		float(64) / mapscale.x,
		float(64) / mapscale.z
	};
	for (const auto &ent : pine->entities) {
		glm::vec2 pos = { ent->position.x / mapscale.x, ent->position.z / mapscale.z };
		int x = floorf(64 * pos.x);
		int y = floorf(64 * pos.y);
		int index = x * 64 + y;
		if (index > 0 && index < boxes.size()) {
			boxes[index].entities.push_back(ent);
		}
	}
	
	// calculate bounding box of cull boxes
	/*
	for (auto &treebox : boxes) {
		float min = std::numeric_limits<float>::max();
		float max = std::numeric_limits<float>::min();
		for (const auto &ent : treebox.entities) {
			if (ent->position.y > max) { max = ent->position.y; };
			if (ent->position.y < min) { min = ent->position.y; };
		}
		treebox.min.y = min;
		treebox.max.y = max;
	}
	*/

	pine->prepare();
}
	
void TreeForest::colorize(const glm::vec3 &sunpos, const glm::vec3 &fog, float fogfact)
{
	sun_position = sunpos;
	fog_color = fog;
	fog_factor = fogfact;
}
	
void TreeForest::display(const Camera *camera) const
{
	// frustum culling
	glm::vec4 planes[6];
	//glm::mat4 projection = glm::perspective(glm::radians(50.f), camera->aspectratio, camera->nearclip, camera->farclip);
	frustum_to_planes(camera->projection * camera->viewing, planes);
	for (auto &treebox : boxes) {
		glm::vec3 min = treebox.min;
		glm::vec3 max = treebox.max;
		bool visible = AABB_in_frustum(min, max, planes);
		// TODO instance render per box vs fill transform buffer here
		for (auto &ent : treebox.entities) {
			ent->visible = visible;
		}
	}

	int instancecount = 0;
	int billboardcount = 0;
	for (const auto &ent : pine->entities) {
		if (instancecount >= pine->entities.size()) {
			break;
		}
		if (ent->visible) {
			glm::mat4 T = glm::translate(glm::mat4(1.f), ent->position);
			glm::mat4 R = glm::mat4(ent->rotation);
			//float scale = scale_dist(gen);
			//glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(scale, scale, scale));
			if (glm::distance(ent->position, camera->position) < 600.f) {
				pine->tbuffer.matrices[instancecount++] = T * R;
			} else {
				pine->boardbuffer.matrices[billboardcount++] = T * R;
			}
		}
	}

	shader.use();
	shader.uniform_mat4("VP", camera->VP);
	shader.uniform_float("FOG_FACTOR", fog_factor);
	shader.uniform_vec3("CAM_POS", camera->position);
	shader.uniform_vec3("SUN_POS", sun_position);
	shader.uniform_vec3("FOG_COLOR", fog_color);

	shader.uniform_bool("INSTANCED", true);

	pine->display(instancecount, billboardcount);
}
