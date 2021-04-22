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

#define POISSON_PROGRESS_INDICATOR 1
#include "extern/poisson/PoissonGenerator.h"

#include "core/geom.h"
#include "core/entity.h"
#include "core/camera.h"
#include "core/image.h"
#include "core/texture.h"
#include "core/mesh.h"
#include "core/model.h"

#include "tree.h"

static const int MAX_TREES = 40000;
static const uint16_t DENSITY_MAP_RES = 256;

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
	density = new Image { DENSITY_MAP_RES, DENSITY_MAP_RES, COLORSPACE_GRAYSCALE };
}

void TreeForest::teardown(void)
{
	delete pine;
	delete density;
}
	
void TreeForest::clear(void)
{
	pine->clear();
}
	
void TreeForest::spawn(const glm::vec3 &mapscale, const FloatImage *heightmap, uint8_t precipitation)
{
	// first clear the forest
	pine->clear();

	// create density map
	FastNoise fastnoise;
	fastnoise.SetSeed(1337);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(0.01f);
	//fastnoise.SetPerturbFrequency(params->height.perturbfreq);
	//fastnoise.SetFractalOctaves(params->height.octaves);
	//fastnoise.SetFractalLacunarity(params->height.lacunarity);
	//fastnoise.SetGradientPerturbAmp(params->height.perturbamp);

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

}
