#include <iostream>
#include <memory>
#include <vector>
#include <random>
#include <algorithm>
#include <array>
#include <map>
#include <chrono>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/poisson/PoissonGenerator.h"

#include "../geometry/geom.h"
#include "../util/image.h"
#include "../module/module.h"
#include "../graphics/texture.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../media.h"
#include "sitegen.h"

#include "landscape.h"

namespace geography {

struct landgen_parameters {
	// mountain ridge
	float ridge_freq;
	float ridge_perturb;
	FastNoise::CellularReturnType ridge_type;
	// detail noise
	FastNoise::FractalType detail_type;
	float detail_freq;
	float detail_perturb_freq;
	int detail_octaves;
	float detail_lacunarity;
	float detail_perturb;
	// erosion
	float sediment_blur;
};

static const float MIN_AMPLITUDE = 0.1F;
static const float MAX_AMPLITUDE = 1.F;

static const float MIN_MOUNTAIN_FREQ = 0.0015F;
static const float MAX_MOUNTAIN_FREQ = 0.005F;
static const float MIN_MOUNTAIN_PERTURB = 20.F;
static const float MAX_MOUNTAIN_PERTURB = 50.F;
static const float MIN_DETAIL_PERTURB = 150.F;
static const float MAX_DETAIL_PERTURB = 200.F;
static const float DETAIL_FREQ = 0.001F;
static const float DETAIL_PERTURB_FREQ = 0.001F;
static const int DETAIL_OCTAVES = 6;
static const float DETAIL_LACUNARITY = 2.5F;
static const float MIN_SEDIMENT_BLUR = 20.F;
static const float MAX_SEDIMENT_BLUR = 25.F;

static const int MAX_TREES = 80000;
static const uint16_t DENSITY_MAP_RES = 256;

static const uint16_t SITEMASK_RES = 2048;

// TODO calculate from AABB
static const float WALL_GATE_DIST = 8.F;
static const float WALL_SEG_DIST = 4.F;
static const float MIN_TOWER_DIFFERENCE = 5.F;

static const std::array<FastNoise::CellularReturnType, 5> RIDGE_TYPES = { FastNoise::Distance, FastNoise::Distance2, FastNoise::Distance2Add, FastNoise::Distance2Sub, FastNoise::Distance2Mul };
static const std::array<FastNoise::FractalType, 3> DETAIL_TYPES = { FastNoise::FBM, FastNoise::Billow, FastNoise::RigidMulti };
static landgen_parameters random_landgen_parameters(int32_t seed);
static bool larger_building(const building_t &a, const building_t &b);
static geom::quadrilateral_t building_box(glm::vec2 center, glm::vec2 halfwidths, float angle);
static bool within_bounds(uint8_t value, const module::bounds_t<uint8_t> &bounds);
static inline glm::quat direction_to_quat(const glm::vec2 &direction);

Landscape::Landscape(const module::Module *mod, uint16_t heightres)
{
	m_module = mod;

	heightmap.resize(heightres, heightres, util::COLORSPACE_GRAYSCALE);
	normalmap.resize(heightres, heightres, util::COLORSPACE_RGB);
	valleymap.resize(heightres, heightres, util::COLORSPACE_RGB);

	container.resize(heightres, heightres, util::COLORSPACE_GRAYSCALE);
	
	density.resize(DENSITY_MAP_RES, DENSITY_MAP_RES, util::COLORSPACE_GRAYSCALE);

	sitemasks.resize(SITEMASK_RES, SITEMASK_RES, util::COLORSPACE_GRAYSCALE);
}

void Landscape::load_buildings()
{
	for (const auto &house_info : m_module->houses) {
		building_group_t building_group;
		building_group.temperature = house_info.temperature;
		building_group.precipitation = house_info.precipitation;
		for (const auto &model_info : house_info.models) {
			auto model = MediaManager::load_model(model_info);
			glm::vec3 size = model->bound_max - model->bound_min;
			building_t house = {
				model,
				size
			};
			building_group.buildings.push_back(house);
		}
		// sort house types from largest to smallest
		std::sort(building_group.buildings.begin(), building_group.buildings.end(), larger_building);
		houses.push_back(building_group);
	}
}
	
void Landscape::clear(void)
{
	heightmap.clear();
	normalmap.clear();
	valleymap.clear();
	container.clear();
	sitemasks.clear();

	m_trees.clear();

	for (auto &group : houses) {
		for (auto &building : group.buildings) {
			building.transforms.clear();
		}
	}
	
	m_walls.clear();
}

void Landscape::generate(long campaign_seed, uint32_t tileref, int32_t local_seed, float amplitude, uint8_t precipitation, uint8_t temperature, uint8_t tree_density, uint8_t site_radius, bool walled, bool nautical)
{
	clear();

	geom::rectangle_t site_scale = {
		{ 0.F, 0.F },
		SITE_BOUNDS.max - SITE_BOUNDS.min
	};
	sitegen.generate(campaign_seed, tileref, site_scale, site_radius);

	create_valleymap();

	gen_heightmap(local_seed, amplitude);

	// create the normalmap
	normalmap.create_normalmap(&heightmap, 32.f);

	// if scene is a town generate the site
	if (site_radius > 0) {
		create_sitemasks(site_radius);
		place_houses(walled, site_radius, local_seed, temperature);
	}

	if (walled) {
		place_walls();
	}

	printf("precipitation %d\n", precipitation);
	printf("tree density %d\n", tree_density);
	if (nautical == false && precipitation > 0 && tree_density > 0) {
		gen_forest(local_seed, precipitation, temperature, tree_density);
	}
}

const util::Image<float>* Landscape::get_heightmap(void) const
{
	return &heightmap;
}
	
const std::vector<tree_t>& Landscape::get_trees(void) const
{
	return m_trees;
}

const std::vector<building_group_t>& Landscape::get_houses(void) const
{
	return houses;
}

const util::Image<uint8_t>* Landscape::get_normalmap(void) const
{
	return &normalmap;
}

const util::Image<uint8_t>* Landscape::get_sitemasks(void) const
{
	return &sitemasks;
}

void Landscape::gen_forest(int32_t seed, uint8_t precipitation, uint8_t temperature, uint8_t tree_density) 
{
	// add valid trees
	for (const auto &tree_info : m_module->vegetation.trees) {
		// find out if tree species is suitable to local climate
		if (within_bounds(precipitation, tree_info.precipitation) && within_bounds(temperature, tree_info.temperature)) {
			// add the tree models to the pool
			for (const auto &model : tree_info.models) {
				tree_t tree;
				tree.trunk = model.trunk;
				tree.leaves = model.leaves;
				tree.billboard = model.billboard;
				m_trees.push_back(tree);
			}
		}
	}

	printf("number of tree types %d\n", m_trees.size());

	// early exit
	if (m_trees.size() < 1) {
		return;
	}
	
	// create density map
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(0.01f);

	density.noise(&fastnoise, glm::vec2(1.f, 1.f), util::CHANNEL_RED);

	// spawn the forest
	glm::vec2 hmapscale = {
		float(heightmap.width()) / SCALE.x,
		float(heightmap.height()) / SCALE.z
	};

	std::random_device rd;
	std::mt19937 gen(seed);
	std::uniform_int_distribution<uint32_t> index_dist(0, m_trees.size()-1);
	std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
	std::uniform_real_distribution<float> scale_dist(1.f, 2.f);
	std::uniform_real_distribution<float> density_dist(0.f, 1.f);

	float rain = precipitation / 255.f;

	printf("tree density %f\n", rain);
	for (const auto &tree : m_trees) {
		std::cout << tree.trunk << std::endl;
	}

	PoissonGenerator::DefaultPRNG PRNG(seed);
	const auto positions = PoissonGenerator::generatePoissonPoints(MAX_TREES, PRNG, false);

	for (const auto &point : positions) {
		glm::vec3 position = { point.x * (SCALE.x * 0.5) + (0.5f * (SCALE.x - (SCALE.x * 0.5))), 0.f, point.y * (SCALE.z * 0.5) + (0.5f * (SCALE.z - (SCALE.z * 0.5))) };
		position.y = sample_heightmap(glm::vec2(position.x,  position.z));
		if (position.y < (0.5f * SCALE.y)) {
			// if tree grows on road remove it
			if (geom::point_in_rectangle(glm::vec2(position.x, position.z), SITE_BOUNDS)) {
				glm::vec2 site_pos = { position.x - SITE_BOUNDS.min.x, position.z - SITE_BOUNDS.min.y };
				site_pos /= (SITE_BOUNDS.max - SITE_BOUNDS.min);
				if (sitemasks.sample(site_pos.x * sitemasks.width(), site_pos.y * sitemasks.height(), util::CHANNEL_RED) > 0) {
					continue;
				}
			}
			float P = density.sample(point.x * density.width(), point.y * density.height(), util::CHANNEL_RED) / 255.f;
			if (P > 0.8f) { P = 1.f; }
			// limit density
			P = glm::clamp(P, 0.f, tree_density / 255.f);
			float R = density_dist(gen);
			if (P < 0.5f) {
				P *= P * P;
			}
			if ( R > P ) { continue; }
			float slope = 1.f - (normalmap.sample(hmapscale.x*position.x, hmapscale.y*position.z, util::CHANNEL_GREEN) / 255.f);
			if (slope > 0.15f) {
				continue;
			}
			position.y -= 1.f;
			glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));
			// to give the appearance that mountains are larger than they really are make the trees smaller if they are on higher elevation
			float scale = scale_dist(gen);
			if (position.y > (0.25f * SCALE.y)) {
				scale = glm::smoothstep(0.4f, 1.f, 1.f - (position.y / SCALE.y));
				scale = glm::clamp(scale, 0.1f, 1.f);
			}
			geom::transformation_t transform = { position, rotation, scale };
			m_trees[index_dist(gen)].transforms.push_back(transform);
		}
	}
}
	
void Landscape::create_valleymap(void)
{
	glm::vec2 scale = SITE_BOUNDS.max - SITE_BOUNDS.min;
	glm::vec2 imagescale = { valleymap.width(), valleymap.height() };

	for (const auto &district : sitegen.districts) {
		uint8_t amp = 255;
		bool near_road = false;
		for (const auto &junction : district.junctions) {
			if (junction->street) {
				near_road = true;
				break;
			}
		}

		if (district.radius < 3 || near_road == true) {
			amp = 64;
		}
		glm::vec2 a = district.center / scale;
		for (const auto &section : district.sections) {
			glm::vec2 b = section->j0->position / scale;
			glm::vec2 c = section->j1->position / scale;
			valleymap.draw_triangle(imagescale * a, imagescale * b, imagescale * c, util::CHANNEL_RED, amp);
		}
	}
	
	// central part of settlement has higher elevation
	glm::vec2 realscale = glm::vec2(SCALE.x, SCALE.z);
	for (const auto &district : sitegen.districts) {
		if (district.radius < 2) {
			glm::vec2 a = (district.center + SITE_BOUNDS.min) / realscale;
			for (const auto &section : district.sections) {
				glm::vec2 b = (section->j0->position + SITE_BOUNDS.min) / realscale;
				glm::vec2 c = (section->j1->position + SITE_BOUNDS.min) / realscale;
				valleymap.draw_triangle(imagescale * a, imagescale * b, imagescale * c, util::CHANNEL_RED, 100);
			}
		}
	}

	valleymap.blur(5.f);
}

void Landscape::gen_heightmap(int32_t seed, float amplitude)
{
	amplitude = glm::clamp(amplitude, MIN_AMPLITUDE, MAX_AMPLITUDE);

	// random noise config
	landgen_parameters params = random_landgen_parameters(seed);

	// first we mix two noise maps for the heightmap
	// the first noise is cellular noise to add mountain ridges
	// the second noise is fractal noise to add overall detail to the heightmap
	FastNoise cellnoise;
	cellnoise.SetSeed(seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(params.ridge_freq);
	cellnoise.SetCellularReturnType(params.ridge_type);
	cellnoise.SetGradientPerturbAmp(params.ridge_perturb);

	FastNoise fractalnoise;
	fractalnoise.SetSeed(seed);
	fractalnoise.SetNoiseType(FastNoise::SimplexFractal);
	fractalnoise.SetFractalType(params.detail_type);
	fractalnoise.SetFrequency(params.detail_freq);
	fractalnoise.SetPerturbFrequency(params.detail_perturb_freq);
	fractalnoise.SetFractalOctaves(params.detail_octaves);
	fractalnoise.SetFractalLacunarity(params.detail_lacunarity);
	fractalnoise.SetGradientPerturbAmp(params.detail_perturb);

	heightmap.noise(&fractalnoise, glm::vec2(1.f, 1.f), util::CHANNEL_RED);
	heightmap.normalize(util::CHANNEL_RED);
	container.cellnoise(&cellnoise, glm::vec2(1.f, 1.f), util::CHANNEL_RED);

	// mix two noise images based on height
	for (int i = 0; i < heightmap.raster().size(); i++) {
		float height = heightmap.raster()[i];
		height *= height;
		float cell = container.raster()[i];
		height = amplitude * glm::mix(height, cell, height);
		heightmap.raster()[i] = height;
	}

	// apply mask
	// apply a mask to lower the amplitude in the center of the map so two armies can fight eachother without having to climb steep cliffs
	//printf("amp %f\n", amplitude);
	if (amplitude > 0.5f) {
		for (int x = 0; x < heightmap.width(); x++) {
			for (int y = 0; y < heightmap.height(); y++) {
				uint8_t valley = valleymap.sample(x, y, util::CHANNEL_RED);
				float height = heightmap.sample(x, y, util::CHANNEL_RED);
				height = (valley / 255.f) * height;
				//height = glm::mix(height, 0.5f*height, masker / 255.f);
				heightmap.plot(x, y, util::CHANNEL_RED, height);
			}
		}
	}

	// apply blur
	// to simulate erosion and sediment we mix a blurred image with the original heightmap based on the height (lower areas receive more blur, higher areas less)
	// this isn't an accurate erosion model but it is fast and looks decent enough
	container.copy(&heightmap);
	container.blur(params.sediment_blur);

	for (int i = 0; i < heightmap.raster().size(); i++) {
		float height = heightmap.raster()[i];
		float blurry = container.raster()[i];
		height = glm::mix(blurry, height, height);
		height = glm::clamp(height, 0.f, 1.f);
		heightmap.raster()[i] = height;
	}
}
	
void Landscape::place_houses(bool walled, uint8_t radius, int32_t seed, uint8_t temperature)
{
	// create wall cull quads so houses don't go through city walls
	std::vector<geom::quadrilateral_t> wall_boxes;

	if (walled) {
		for (const auto &d : sitegen.districts) {
			for (const auto &sect : d.sections) {
				if (sect->wall) {
					geom::segment_t S = { sect->d0->center, sect->d1->center };
					glm::vec2 mid = geom::segment_midpoint(S.P0, S.P1);
					glm::vec2 direction = glm::normalize(S.P1 - S.P0);
					float angle = atan2(direction.x, direction.y);
					glm::vec2 halfwidths = { 10.f, glm::distance(mid, S.P1) };
					geom::quadrilateral_t box = building_box(mid, halfwidths, angle);
					wall_boxes.push_back(box);
				}
			}
		}
	}

	std::vector<building_t*> buildings_pool;
	for (auto &group : houses) {
		if (within_bounds(temperature, group.temperature)) {
			for (auto &building : group.buildings) {
				buildings_pool.push_back(&building);	
			}
			break; // pick the first valid group
		}
	}
	//printf("n house templates %d\n", houses.size());

	// no valid houses found
	if (buildings_pool.size() < 1) { return; }

	// place the town houses
	for (const auto &d : sitegen.districts) {
		for (const auto &parc : d.parcels) {
			float front = glm::distance(parc.quad.b, parc.quad.a);
			float back = glm::distance(parc.quad.c, parc.quad.d);
			float left = glm::distance(parc.quad.b, parc.quad.c);
			float right = glm::distance(parc.quad.a, parc.quad.d);
			float angle = atan2(parc.direction.x, parc.direction.y);
			glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.f, 1.f, 0.f));
			float height = sample_heightmap(parc.centroid + SITE_BOUNDS.min);
			glm::vec3 position = { parc.centroid.x+SITE_BOUNDS.min.x, height, parc.centroid.y+SITE_BOUNDS.min.y };
			for (auto &house : buildings_pool) {
				if ((front > house->bounds.x && back > house->bounds.x) && (left > house->bounds.z && right > house->bounds.z)) {
					glm::vec2 halfwidths = {  0.5f*house->bounds.x, 0.5f*house->bounds.z };
					geom::quadrilateral_t box = building_box(parc.centroid, halfwidths, angle);
					bool valid = true;
					if (walled) {
						for (auto &wallbox : wall_boxes) {
							if (geom::quad_quad_intersection(box, wallbox)) {
								valid = false;
								break;
							}
						}
					}
					if (valid) {
						geom::transformation_t transform = { position, rotation, 1.f };
						house->transforms.push_back(transform);
					}

					break;
				}
			}
		}
	}

	// place some farm houses outside walls
	if (buildings_pool.size() > 0) {
		std::random_device rd;
		std::mt19937 gen(seed);
		std::uniform_real_distribution<float> rot_dist(0.f, 360.f);
		std::uniform_int_distribution<uint32_t> house_type_dist(0, buildings_pool.size()-1);

		std::bernoulli_distribution bern(0.25f);

		for (const auto &district : sitegen.districts) {
			if (district.radius > radius && bern(gen) == true) {
				float height = sample_heightmap(district.center + SITE_BOUNDS.min);
				glm::vec3 position = { district.center.x+SITE_BOUNDS.min.x, height, district.center.y+SITE_BOUNDS.min.y };
				glm::quat rotation = glm::angleAxis(glm::radians(rot_dist(gen)), glm::vec3(0.f, 1.f, 0.f));
				// pick a random house
				auto &house = buildings_pool[house_type_dist(gen)];
				geom::transformation_t transform = { position, rotation, 1.f };
				house->transforms.push_back(transform);
			}
		}
	}
}
	
void Landscape::create_sitemasks(uint8_t radius)
{
	glm::vec2 scale = SITE_BOUNDS.max - SITE_BOUNDS.min;
	glm::vec2 imagescale = { sitemasks.width(), sitemasks.height() };

	for (const auto &district : sitegen.districts) {
		for (const auto &parcel : district.parcels) {
			glm::vec2 street = parcel.centroid + parcel.direction;
			float min = std::numeric_limits<float>::max();
			for (const auto &edge : district.sections) {
				glm::vec2 p = geom::closest_point_segment(parcel.centroid, edge->j0->position, edge->j1->position);
				float dist = glm::distance(p, parcel.centroid);
				if (dist < min) {
					min = dist;
					street = p;
				}
			}
			glm::vec2 a = parcel.centroid / scale;
			glm::vec2 b = street / scale;
			sitemasks.draw_thick_line(a.x * sitemasks.width(), a.y * sitemasks.height(), b.x * sitemasks.width(), b.y * sitemasks.height(), 6, util::CHANNEL_RED, 200);
		}
	}
	sitemasks.blur(5.f);
	
	// site road mask
	for (const auto &road : sitegen.highways) {
		glm::vec2 a = road.P0 / scale;
		glm::vec2 b = road.P1 / scale;
		sitemasks.draw_thick_line(a.x * sitemasks.width(), a.y * sitemasks.height(), b.x * sitemasks.width(), b.y * sitemasks.height(), 3, util::CHANNEL_RED, 255);
	}

	for (const auto &sect : sitegen.sections) {
		if (sect.j0->radius < radius && sect.j1->radius < radius) {
			if (sect.j0->border == false && sect.j1->border == false) {
				glm::vec2 a = sect.j0->position / scale;
				glm::vec2 b = sect.j1->position / scale;
				sitemasks.draw_thick_line(a.x * sitemasks.width(), a.y * sitemasks.height(), b.x * sitemasks.width(), b.y * sitemasks.height(), 3, util::CHANNEL_RED, 210);
			}
		}
	}

	// walls
	for (const auto &wall : sitegen.walls) {
		glm::vec2 a = wall.S.P0 / scale;
		glm::vec2 b = wall.S.P1 / scale;
		sitemasks.draw_thick_line(a.x * sitemasks.width(), a.y * sitemasks.height(), b.x * sitemasks.width(), b.y * sitemasks.height(), 10, util::CHANNEL_RED, 210);
	}

	sitemasks.blur(1.f);
}
	
void Landscape::place_walls()
{
	// place the wall gates
	wall_t gate;
	gate.model = m_module->fortifications.wood.gate;
	for (const auto &wall : sitegen.walls) {
		if (wall.gate) {
			glm::vec2 mid = geom::segment_midpoint(wall.S.P0, wall.S.P1);
			glm::vec2 direction = glm::normalize(wall.S.P1 - wall.S.P0);
			glm::quat rotation = direction_to_quat(direction);
			float h = sample_heightmap(mid +  SITE_BOUNDS.min);
			glm::vec2 leftorigin = wall.S.P0;
			geom::transformation_t transform = {
				glm::vec3(mid.x+SITE_BOUNDS.min.x, h, mid.y+SITE_BOUNDS.min.y),
				rotation,
				1.f
			};
			gate.transforms.push_back(transform);
		}
	}

	// place the wall towers
	wall_t tower;
	tower.model = m_module->fortifications.wood.tower;
	for (const auto &wall : sitegen.walls) {
		glm::vec2 direction = glm::normalize(wall.S.P1 - wall.S.P0);
		glm::quat rotation = direction_to_quat(direction);
		glm::vec3 position = { wall.S.P0.x + SITE_BOUNDS.min.x, 0.f,  wall.S.P0.y + SITE_BOUNDS.min.y};
		position.y = sample_heightmap(wall.S.P0 + SITE_BOUNDS.min);
		geom::transformation_t transform = { position, rotation, 1.f };
		tower.transforms.push_back(transform);
	}

	wall_t segment_standard;
	segment_standard.model = m_module->fortifications.wood.segment;
	wall_t segment_left;
	segment_left.model = m_module->fortifications.wood.segment_left;
	wall_t segment_right;
	segment_right.model = m_module->fortifications.wood.segment_right;
	wall_t segment_both;
	segment_both.model = m_module->fortifications.wood.segment_both;
	wall_t segment_ramp;
	segment_ramp.model = m_module->fortifications.wood.ramp;
	// fill the walls
	for (const auto &wall : sitegen.walls) {
		if (wall.gate) {
			glm::vec2 direction = glm::normalize(wall.S.P1 - wall.S.P0);
			glm::vec2 mid = geom::segment_midpoint(wall.S.P0, wall.S.P1);
			geom::segment_t left = { wall.S.P0, mid - (WALL_GATE_DIST*direction) };
			fill_wall(left, &segment_standard, &segment_left, &segment_right, &segment_both, &segment_ramp);
			geom::segment_t right = { mid + (WALL_GATE_DIST*direction), wall.S.P1 };
			fill_wall(right, &segment_standard, &segment_left, &segment_right, &segment_both, &segment_ramp);
		} else {
			fill_wall(wall.S, &segment_standard, &segment_left, &segment_right, &segment_both, &segment_ramp);
		}
	}

	m_walls.push_back(gate);
	m_walls.push_back(tower);
	m_walls.push_back(segment_standard);
	m_walls.push_back(segment_left);
	m_walls.push_back(segment_right);
	m_walls.push_back(segment_both);
	m_walls.push_back(segment_ramp);
}
	
void Landscape::fill_wall(geom::segment_t S, wall_t *wall, wall_t *wall_left, wall_t *wall_right, wall_t *wall_both, wall_t *ramp)
{
	glm::vec2 direction = glm::normalize(S.P1 - S.P0);
	glm::quat rotation = direction_to_quat(direction);

	glm::vec2 origin = S.P0;
	float d = (1.f / WALL_SEG_DIST) * glm::distance(S.P0, S.P1);
	float h0 = sample_heightmap(S.P0 + SITE_BOUNDS.min);
	float h1 = sample_heightmap(S.P1 + SITE_BOUNDS.min);
	glm::vec3 position = { origin.x + SITE_BOUNDS.min.x, 0.f, origin.y + SITE_BOUNDS.min.y };
	position.y = sample_heightmap(glm::vec2(position.x, position.z));
	if (fabs(h0 - h1) < MIN_TOWER_DIFFERENCE) {
		float height = position.y;
		for (int i = 0; i < int(d); i++) {
			glm::vec3 position = { origin.x + SITE_BOUNDS.min.x, height, origin.y + SITE_BOUNDS.min.y };
			geom::transformation_t transform = { position, rotation, 1.f };
			if (i == int(d)/2) {
				ramp->transforms.push_back(transform);
			} else {
				wall->transforms.push_back(transform);
			}
			origin += WALL_SEG_DIST * direction;
		}
	} else {
		for (int i = 0; i < int(d); i++) {
			glm::vec2 realspace = origin + SITE_BOUNDS.min;
			glm::vec3 position = { realspace.x, 0.f, realspace.y };
			position.y = sample_heightmap(glm::vec2(position.x, position.z));
			glm::vec2 offset = WALL_SEG_DIST * direction;
			geom::transformation_t transform = { position, rotation, 1.f };
			if (i == int(d)/2) {
				ramp->transforms.push_back(transform);
			} else {
				float left = sample_heightmap(realspace - offset);
				float right = sample_heightmap(realspace + offset);
				if (left < position.y && right < position.y) {
					wall_both->transforms.push_back(transform);
				} else if (left < position.y) {
					wall_left->transforms.push_back(transform);
				} else if (right < position.y) {
					wall_right->transforms.push_back(transform);
				} else {
					wall->transforms.push_back(transform);
				}
			}
			origin += offset;
		}
	}
}

float Landscape::sample_heightmap(const glm::vec2 &real) const
{
	glm::vec2 imagespace = {
		heightmap.width() * (real.x / SCALE.x),
		heightmap.height() * (real.y / SCALE.z)
	};
	return SCALE.y * heightmap.sample(roundf(imagespace.x), roundf(imagespace.y), util::CHANNEL_RED);
}
	
static landgen_parameters random_landgen_parameters(int32_t seed)
{
	std::mt19937 gen(seed);

	std::uniform_real_distribution<float> mountain_freq_distrib(MIN_MOUNTAIN_FREQ, MAX_MOUNTAIN_FREQ);
	std::uniform_real_distribution<float> mountain_perturb_distrib(MIN_MOUNTAIN_PERTURB, MAX_MOUNTAIN_PERTURB);
	std::uniform_int_distribution<uint32_t> mountain_type_distrib(0, RIDGE_TYPES.size()-1);

	std::uniform_int_distribution<uint32_t> detail_type_distrib(0, DETAIL_TYPES.size()-1);
	std::uniform_real_distribution<float> detail_perturb_distrib(MIN_DETAIL_PERTURB, MAX_DETAIL_PERTURB);

	std::uniform_real_distribution<float> sediment_blur_distrib(MIN_SEDIMENT_BLUR, MAX_SEDIMENT_BLUR);

	landgen_parameters params;

	//params.seed = noise_seed_distrib(gen);

	params.ridge_freq = mountain_freq_distrib(gen);
	params.ridge_perturb = mountain_perturb_distrib(gen);
	params.ridge_type = RIDGE_TYPES[mountain_type_distrib(gen)];

	params.detail_type = DETAIL_TYPES[detail_type_distrib(gen)];
	params.detail_freq = DETAIL_FREQ;
	params.detail_perturb_freq = DETAIL_PERTURB_FREQ;
	params.detail_octaves = DETAIL_OCTAVES;
	params.detail_lacunarity = DETAIL_LACUNARITY;
	params.detail_perturb = detail_perturb_distrib(gen);

	params.sediment_blur = sediment_blur_distrib(gen);

	return params;
}

static bool larger_building(const building_t &a, const building_t &b)
{
	return (a.bounds.x * a.bounds.z) > (b.bounds.x * b.bounds.z);
}

static geom::quadrilateral_t building_box(glm::vec2 center, glm::vec2 halfwidths, float angle)
{
	glm::vec2 a = {-halfwidths.x, halfwidths.y};
	glm::vec2 b = {-halfwidths.x, -halfwidths.y};
	glm::vec2 c = {halfwidths.x, -halfwidths.y};
	glm::vec2 d = {halfwidths.x, halfwidths.y};

	glm::mat2 R = {
		cos(angle), -sin(angle),
		sin(angle), cos(angle)
	};

	geom::quadrilateral_t quad = {
		center + R * a,
		center + R * b,
		center + R * c,
		center + R * d
	};

	return quad;
}

static bool within_bounds(uint8_t value, const module::bounds_t<uint8_t> &bounds)
{
	return (value >= bounds.min && value <= bounds.max);
}

static inline glm::quat direction_to_quat(const glm::vec2 &direction)
{
	float angle = atan2(direction.x, direction.y);
	glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.f, 1.f, 0.f));

	return rotation;
}

};
