#include <memory>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <map>
#include <random>
#include <vector>
#include <list>
#include <span>
#include <array>

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

// don't move this
#include "extern/namegen/namegen.h"

#include "extern/recast/Recast.h"
#include "extern/recast/DetourNavMesh.h"
#include "extern/recast/DetourNavMeshQuery.h"
#include "extern/recast/ChunkyTriMesh.h"
#include "extern/recast/DetourCrowd.h"

#include "extern/cereal/types/unordered_map.hpp"
#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"
#include "extern/cereal/archives/json.hpp"
#include "extern/cereal/archives/binary.hpp"

#include "extern/ozz/animation/runtime/animation.h"
#include "extern/ozz/animation/runtime/local_to_model_job.h"
#include "extern/ozz/animation/runtime/sampling_job.h"
#include "extern/ozz/animation/runtime/skeleton.h"
#include "extern/ozz/base/maths/soa_transform.h"

// define this to make bullet and ozz animation compile on Windows
#define BT_NO_SIMD_OPERATOR_OVERLOADS
#include "extern/bullet/btBulletDynamicsCommon.h"
#include "bullet/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "extern/bullet/BulletCollision/CollisionDispatch/btGhostObject.h"

#include "extern/inih/INIReader.h"

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "extern/freetype/freetype-gl.h"

#include "extern/aixlog/aixlog.h"

#include "geometry/geom.h"
#include "geometry/voronoi.h"
#include "util/image.h"
#include "util/entity.h"
#include "util/camera.h"
#include "util/window.h"
#include "util/input.h"
#include "util/timer.h"
#include "util/animation.h"
#include "util/navigation.h"
#include "module/module.h"
#include "graphics/text.h"
#include "graphics/shader.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "graphics/model.h"
#include "graphics/clouds.h"
#include "graphics/sky.h"
#include "graphics/render.h"
#include "graphics/terrain.h"
#include "graphics/label.h"
#include "graphics/forest.h"
#include "physics/heightfield.h"
#include "physics/physics.h"
#include "physics/bumper.h"
#include "physics/ragdoll.h"
#include "media.h"
#include "object.h"
#include "creature.h"
#include "debugger.h"
#include "geography/sitegen.h"
#include "geography/landscape.h"
#include "crowd.h"
#include "army.h"
#include "battle.h"

static const geom::rectangle_t AGENT_NAV_AREA = {
	{ 2560.F, 2560.F },
	{ 3584.F, 3584.F }
};

static void heightmap_to_triangle_soup(const util::Image<float> *heightmap, float amp, std::vector<float> &vertices, std::vector<int> &indices, float x_offset, float y_offset, float scale);

void Battle::init(const module::Module *mod, const util::Window *window, const shader_group_t *shaders)
{
	landscape = std::make_unique<geography::Landscape>(mod, 2048);

	terrain = std::make_unique<gfx::Terrain>(landscape->SCALE, landscape->get_heightmap(), landscape->get_normalmap(), landscape->get_sitemasks());

	load_assets(mod);

	ordinary = std::make_unique<gfx::RenderGroup>(&shaders->debug);
	creatures = std::make_unique<gfx::RenderGroup>(&shaders->creature);

	skybox.init(window->width, window->height);
	
	forest = std::make_unique<gfx::Forest>(&shaders->tree, &shaders->billboard);
	
	central_heightmap.resize(128, 128, util::COLORSPACE_GRAYSCALE);
}

void Battle::load_assets(const module::Module *mod)
{
	landscape->load_buildings();
	
	terrain->insert_material("STONEMAP", MediaManager::load_texture("ground/stone.dds"));
	terrain->insert_material("REGOLITH_MAP", MediaManager::load_texture("ground/grass.dds"));
	terrain->insert_material("GRAVELMAP", MediaManager::load_texture("ground/gravel.dds"));
	terrain->insert_material("DETAILMAP", MediaManager::load_texture("ground/stone_normal.dds"));
	terrain->insert_material("WAVE_BUMPMAP", MediaManager::load_texture("ground/water_normal.dds"));
}
	
void Battle::add_entities(const module::Module *mod)
{
	add_trees();
	add_buildings();
	add_walls();
	add_creatures(mod);

	add_physics_bodies();
}
	
void Battle::add_creatures(const module::Module *mod)
{
	glm::vec3 end = glm::vec3(2700.f, 0.f, 3000.f);
	glm::vec3 origin = { end.x, landscape->SCALE.y, end.z };
	auto result = physicsman.cast_ray(origin, end, physics::COLLISION_GROUP_HEIGHTMAP | physics::COLLISION_GROUP_WORLD);
	if (result.hit) {
		origin = result.point;
		origin.y += 1.f;
	}
	player = new Creature { origin, glm::quat(1.f, 0.f, 0.f, 0.f), MediaManager::load_model("human.glb"), mod->test_armature };

	physicsman.add_body(player->get_body(), physics::COLLISION_GROUP_ACTOR, physics::COLLISION_GROUP_ACTOR | physics::COLLISION_GROUP_WORLD | physics::COLLISION_GROUP_HEIGHTMAP);

	std::vector<const Entity*> ents;
	ents.push_back(player);
	creatures->add_object(MediaManager::load_model("human.glb"), ents);

	glm::vec3 target_end = glm::vec3(3072.f, 0.f, 3072.f);
	glm::vec3 target_origin = { target_end.x, landscape->SCALE.y, target_end.z };
	result = physicsman.cast_ray(target_origin, target_end, physics::COLLISION_GROUP_HEIGHTMAP | physics::COLLISION_GROUP_WORLD);
	crowd_manager->add_agent(player->position, result.point, navigation.get_navquery());
}
	
void Battle::add_buildings()
{
	// add houses
	const auto house_groups = landscape->get_houses();
	for (const auto &group : house_groups) {
		for (const auto &house : group.buildings) {
			std::vector<const Entity*> house_entities;
			const gfx::Model *model = house.model;
			// get collision shape
			std::vector<glm::vec3> positions;
			std::vector<uint16_t> indices;
			uint16_t offset = 0;
			for (const auto &mesh : model->collision_trimeshes) {
				for (const auto &pos : mesh.positions) {
					positions.push_back(pos);
				}
				for (const auto &index : mesh.indices) {
					indices.push_back(index + offset);
				}
				offset = positions.size();
			}
			btCollisionShape *shape = physicsman.add_mesh(positions, indices);
			// create entities
			for (const auto &transform : house.transforms) {
				StationaryObject *stationary = new StationaryObject { transform.position, transform.rotation, shape };
				stationaries.push_back(stationary);
				house_entities.push_back(stationary);
			}

			ordinary->add_object(model, house_entities);
		}
	}
}
	
void Battle::add_walls()
{
	const auto walls = landscape->get_walls();
	for (const auto &wall : walls) {
		std::vector<const Entity*> wall_entities;
		const auto model = MediaManager::load_model(wall.model);
		// get collision shape
		std::vector<glm::vec3> positions;
		std::vector<uint16_t> indices;
		uint16_t offset = 0;
		for (const auto &mesh : model->collision_trimeshes) {
			for (const auto &pos : mesh.positions) {
				positions.push_back(pos);
			}
			for (const auto &index : mesh.indices) {
				indices.push_back(index + offset);
			}
			offset = positions.size();
		}
		btCollisionShape *shape = physicsman.add_mesh(positions, indices);
		// create entities
		for (const auto &transform : wall.transforms) {
			StationaryObject *stationary = new StationaryObject { transform.position, transform.rotation, shape };
			stationaries.push_back(stationary);
			wall_entities.push_back(stationary);
		}

		ordinary->add_object(model, wall_entities);
	}
}
	
void Battle::add_trees()
{
	const auto trees = landscape->get_trees();

	entities.clear();

	for (const auto &tree : trees) {
		const gfx::Model *trunk = MediaManager::load_model(tree.trunk);
		const gfx::Model *leaves = MediaManager::load_model(tree.leaves);
		const gfx::Model *billboard = MediaManager::load_model(tree.billboard);
		// get collision shape
		std::vector<glm::vec3> positions;
		std::vector<uint16_t> indices;
		uint16_t offset = 0;
		for (const auto &mesh : trunk->collision_trimeshes) {
			for (const auto &pos : mesh.positions) {
				positions.push_back(pos);
			}
			for (const auto &index : mesh.indices) {
				indices.push_back(index + offset);
			}
			offset = positions.size();
		}
		btCollisionShape *shape = physicsman.add_mesh(positions, indices);
		auto start = entities.size();
		for (const auto &transform : tree.transforms) {
			if (point_in_rectangle(glm::vec2(transform.position.x, transform.position.z), landscape->SITE_BOUNDS)) {
				StationaryObject *stationary = new StationaryObject { transform.position, transform.rotation, shape };
				tree_stationaries.push_back(stationary);
			}
		}
		std::vector<const geom::transformation_t*> transformations;
		for (int i = 0; i < tree.transforms.size(); i++) {
			transformations.push_back(&tree.transforms[i]);
		}
		forest->add_model(trunk, leaves, billboard, transformations);
	}

	forest->build_hierarchy();
}

void Battle::add_physics_bodies()
{
	for (const auto &stationary : tree_stationaries) {
		physicsman.add_body(stationary->body, physics::COLLISION_GROUP_WORLD, physics::COLLISION_GROUP_ACTOR | physics::COLLISION_GROUP_RAY | physics::COLLISION_GROUP_RAGDOLL);
	}

	// add stationary objects to physics
	for (const auto &stationary : stationaries) {
		physicsman.add_body(stationary->body, physics::COLLISION_GROUP_WORLD, physics::COLLISION_GROUP_ACTOR | physics::COLLISION_GROUP_RAY | physics::COLLISION_GROUP_RAGDOLL);
	}
}

void Battle::remove_physics_bodies()
{
	// remove stationary objects from physics
	for (const auto &stationary : stationaries) {
		physicsman.remove_body(stationary->body);
	}
	for (const auto &stationary : tree_stationaries) {
		physicsman.remove_body(stationary->body);
	}

	physicsman.remove_body(player->get_body());
	if (player->m_ragdoll_mode) {
		player->remove_ragdoll(physicsman.get_world());
	}
}

void Battle::cleanup()
{
	navigation.cleanup();

	remove_physics_bodies();

	delete player;

	physicsman.clear();
	ordinary->clear();
	creatures->clear();

	// delete stationaries
	for (int i = 0; i < stationaries.size(); i++) {
		delete stationaries[i];
	}
	stationaries.clear();
	for (int i = 0; i < tree_stationaries.size(); i++) {
		delete tree_stationaries[i];
	}
	tree_stationaries.clear();

	forest->clear();
}

void Battle::teardown()
{
	skybox.teardown();

	physicsman.clear();
}
	
void Battle::create_navigation()
{
	std::vector<float> vertex_soup;
	std::vector<int> index_soup;

	// add static collision like buildings and walls
	const auto walls = landscape->get_walls();
	for (const auto &wall : walls) {
		const auto model = MediaManager::load_model(wall.model);
		for (const auto &transform : wall.transforms) {
			glm::mat4 T = glm::translate(glm::mat4(1.f), transform.position);
			glm::mat4 R = glm::mat4(transform.rotation);
			glm::mat4 M = T * R;
			int index_offset = vertex_soup.size() / 3;
			for (const auto &mesh : model->collision_trimeshes) {
				for (const auto &pos : mesh.positions) {
					//positions.push_back(pos);
					glm::vec4 vertex = {pos.x, pos.y, pos.z, 1.f};
					vertex = M * vertex;
					vertex_soup.push_back(vertex.x);
					vertex_soup.push_back(vertex.y);
					vertex_soup.push_back(vertex.z);
				}
				for (const auto &index : mesh.indices) {
					index_soup.push_back(index_offset + index);
				}
			}
		}
	}
	const auto house_groups = landscape->get_houses();
	for (const auto &group : house_groups) {
		for (const auto &house : group.buildings) {
			//const auto model = MediaManager::load_model(house.model);
			const auto model = house.model;
			for (const auto &transform : house.transforms) {
				glm::mat4 T = glm::translate(glm::mat4(1.f), transform.position);
				glm::mat4 R = glm::mat4(transform.rotation);
				glm::mat4 M = T * R;
				int index_offset = vertex_soup.size() / 3;
				for (const auto &mesh : model->collision_trimeshes) {
					for (const auto &pos : mesh.positions) {
						//positions.push_back(pos);
						glm::vec4 vertex = {pos.x, pos.y, pos.z, 1.f};
						vertex = M * vertex;
						vertex_soup.push_back(vertex.x);
						vertex_soup.push_back(vertex.y);
						vertex_soup.push_back(vertex.z);
					}
					for (const auto &index : mesh.indices) {
						index_soup.push_back(index_offset + index);
					}
				}
			}
		}
	}
	
	// add the heightmap
	central_heightmap.clear();
	const glm::vec2 downscale = { 
		(AGENT_NAV_AREA.max.x - AGENT_NAV_AREA.min.x) / float(central_heightmap.width()),
		(AGENT_NAV_AREA.max.y - AGENT_NAV_AREA.min.y) / float(central_heightmap.height())
	};
	for (int y = 0; y < central_heightmap.width(); y++) {
		for (int x = 0; x < central_heightmap.height(); x++) {
			glm::vec2 realspace = {
				AGENT_NAV_AREA.min.x + downscale.x * x,
				AGENT_NAV_AREA.min.y + downscale.y * y
			};
			float height = landscape->sample_heightmap(realspace) / landscape->SCALE.y;
			central_heightmap.plot(x, y, util::CHANNEL_RED, height);
		}
	}

	heightmap_to_triangle_soup(&central_heightmap, landscape->SCALE.y, vertex_soup, index_soup, AGENT_NAV_AREA.min.x, AGENT_NAV_AREA.min.y, (AGENT_NAV_AREA.max.x-AGENT_NAV_AREA.min.x)/float(central_heightmap.width()));
	
	navigation.build(vertex_soup, index_soup);

	crowd_manager = std::make_unique<CrowdManager>(navigation.get_navmesh());
}


// TODO replace with hmmm triangulator
static void heightmap_to_triangle_soup(const util::Image<float> *heightmap, float amp, std::vector<float> &vertices, std::vector<int> &indices, float x_offset, float y_offset, float scale)
{
	uint32_t index_offset = vertices.size() / 3;

	for (int y = 0; y < heightmap->height(); y++) {
		for (int x = 0; x < heightmap->width(); x++) {
			vertices.push_back(scale * x + x_offset);
			vertices.push_back(heightmap->sample(x, y, util::CHANNEL_RED)*amp);
			vertices.push_back(scale * y + y_offset);
		}
	}
	for (int y = 0; y < heightmap->height()-1; y++) {
		for (int x = 0; x < heightmap->width()-1; x++) {
			int index = x + y * heightmap->height() + index_offset;

			indices.push_back(index);
			indices.push_back(index + heightmap->height());
			indices.push_back(index + 1);

			indices.push_back(index + 1);
			indices.push_back(index + heightmap->height());
			indices.push_back(index + 1 + heightmap->height());
		}
	}
}
