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
#include "../core/entity.h"
//#include "../core/shader.h"
#include "../core/camera.h"
#include "../core/image.h"
#include "../core/texture.h"
#include "../core/mesh.h"
#include "../core/model.h"

#include "tree.h"

TreeKind::TreeKind(const GLTF::Model *modl)
{
	model = modl;
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
}
	
void TreeKind::add(const glm::vec3 &position, const glm::quat &rotation)
{
	Entity *ent = new Entity { position, rotation };
	entities.push_back(ent);
}

void TreeForest::init(const GLTF::Model *pinemodel)
{
	pine = new TreeKind { pinemodel };
}

void TreeForest::teardown(void)
{
	delete pine;
}
	
void TreeForest::clear(void)
{
	pine->clear();
}
	
void TreeForest::spawn(const glm::vec3 &mapscale, const FloatImage *heightmap)
{
	// first clear the forest
	pine->clear();

	// spawn the forest
	glm::vec2 hmapscale = {
		float(heightmap->width) / mapscale.x,
		float(heightmap->height) / mapscale.z
	};

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
	std::uniform_real_distribution<float> scale_dist(1.f, 2.f);

	glm::vec2 min = { 1024.f, 1024.f };
	glm::vec2 max = { 5120.f, 5120.f };
	std::uniform_real_distribution<float> map_x(min.x, max.x);
	std::uniform_real_distribution<float> map_y(-0.05f, 0.05f);
	std::uniform_real_distribution<float> map_z(min.y, max.y);
	for (int i = 0; i < 10000; i++) {
		glm::vec3 position = { map_x(gen), 0.f, map_z(gen) };
		position.y = mapscale.y * heightmap->sample(hmapscale.x*position.x, hmapscale.y*position.z, CHANNEL_RED);
		glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));
		pine->add(position, rotation);

		//float scale = scale_dist(gen);
	}
}
