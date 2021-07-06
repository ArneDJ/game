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
#include "graphics/worldmap.h"
#include "graphics/label.h"
#include "physics/heightfield.h"
#include "physics/physics.h"
#include "media.h"
#include "debugger.h"
#include "geography/terragen.h"
#include "geography/worldgraph.h"
#include "geography/mapfield.h"
#include "geography/atlas.h"
#include "army.h"
#include "campaign.h"

// store navigation data
struct navigation_tile_record {
	int x;
	int y;
	std::vector<uint8_t> data;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(x, y, data);
	}
};

struct navigation_mesh_record {
	glm::vec3 origin; 
	float tilewidth; 
	float tileheight; 
	int maxtiles; 
	int maxpolys;
	std::vector<struct navigation_tile_record> tilemeshes;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(origin.x, origin.y, origin.z, tilewidth, tileheight, maxtiles, maxpolys, tilemeshes);
	}
};

void Campaign::init(const util::Window *window, const shader_group_t *shaders)
{
	const auto terragen = atlas.get_terragen();
	worldmap = std::make_unique<gfx::Worldmap>(atlas.SCALE, &terragen->heightmap, atlas.get_watermap(), &terragen->rainmap, atlas.get_materialmasks(), atlas.get_factions());

	ordinary = std::make_unique<gfx::RenderGroup>(&shaders->debug);
	creatures = std::make_unique<gfx::RenderGroup>(&shaders->object);

	player = std::make_unique<ArmyNode>(player_army.position, 40.f);
	
	skybox.init(window->width, window->height);
	
	labelman = std::make_unique<gfx::LabelManager>("fonts/diablo.ttf", 30);

	load_assets();
}
	
void Campaign::load_assets()
{
	worldmap->add_material("STONEMAP", MediaManager::load_texture("ground/stone.dds"));
	worldmap->add_material("SANDMAP", MediaManager::load_texture("ground/sand.dds"));
	worldmap->add_material("SNOWMAP", MediaManager::load_texture("ground/snow.dds"));
	worldmap->add_material("GRASSMAP", MediaManager::load_texture("ground/grass.dds"));
	worldmap->add_material("WATER_BUMPMAP", MediaManager::load_texture("ground/water_normal.dds"));
}
	
void Campaign::add_heightfields()
{
	const auto terragen = atlas.get_terragen();

	int group = physics::COLLISION_GROUP_HEIGHTMAP;
	int masks = physics::COLLISION_GROUP_RAY;
	collisionman.add_heightfield(&terragen->heightmap, atlas.SCALE, group, masks);
	collisionman.add_heightfield(atlas.get_watermap(), atlas.SCALE, group, masks);
}
	
void Campaign::add_armies()
{
	player->teleport(player_army.position);
	player->scale = 2.f;

	std::vector<const Entity*> ents;
	ents.push_back(player.get());

	creatures->add_object(MediaManager::load_model("duck.glb"), ents);
}

void Campaign::add_trees()
{
	auto trees = atlas.get_trees();

	std::vector<const Entity*> ents;

	for (int i = 0; i < trees.size(); i++) {
		Entity *entity = new Entity(trees[i].position, trees[i].rotation);
		entity->scale = trees[i].scale;
		entities.push_back(entity);
		ents.push_back(entity);
	}

	creatures->add_object(MediaManager::load_model("map/pines.glb"), ents);
}

void Campaign::spawn_settlements()
{
	for (const auto &tile : atlas.get_worldgraph()->tiles) {
		if (tile.feature == geography::tile_feature::SETTLEMENT) {
			glm::vec3 position = { tile.center.x, 0.f, tile.center.y };
			geom::transformation_t transform = {
				position,
				{ 1.f, 0.f, 0.f, 0.f },
				1.f
			};
			settlement_t settlement;
			settlement.tileID = tile.index;
			settlement.transform = transform;

			settlement.population = tile.river ? 3 : 1;

			settlements[tile.index] = settlement;
		}
	}

	// name settlements
	std::ifstream t("modules/native/names/town.txt");
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string pattern;
	for (auto i = 0; i < buffer.str().size(); i++) {
		char c = buffer.str().at(i);
		if (c != '\n') {
			pattern.append(1, c);
		}
	}
	//NameGen::Generator namegen(pattern.c_str());
	NameGen::Generator namegen(MIDDLE_EARTH);
	for (auto &settlement : settlements) {
		settlement.second.name = namegen.toString();
	}
}

void Campaign::add_settlements()
{
	for (const auto &settlement : settlements) {
		glm::vec3 position = settlement.second.transform.position;
		position.y = 0.f;
		glm::vec3 origin = { position.x, atlas.SCALE.y, position.z };
		auto result = collisionman.cast_ray(origin, position, physics::COLLISION_GROUP_HEIGHTMAP);
		position.y = result.point.y;
		auto node = std::make_unique<SettlementNode>(position, settlement.second.transform.rotation, settlement.second.tileID);
		collisionman.add_ghost_object(node->ghost_object.get(), physics::COLLISION_GROUP_GHOSTS, physics::COLLISION_GROUP_GHOSTS);
		settlement_nodes.push_back(std::move(node));
		
		// labels
		labelman->add(settlement.second.name, glm::vec3(1.f), position + glm::vec3(0.f, 8.f, 0.f));
	}

	std::vector<const Entity*> ents;
	for (int i = 0; i < settlement_nodes.size(); i++) {
		ents.push_back(settlement_nodes[i].get());
	}

	ordinary->add_object(MediaManager::load_model("map/fort.glb"), ents);
}
	
void Campaign::cleanup()
{
	for (int i = 0; i < entities.size(); i++) {
		delete entities[i];
	}
	entities.clear();

	for (auto &node : settlement_nodes) {
		collisionman.remove_ghost_object(node->ghost_object.get());
	}
	settlement_nodes.clear();

	settlements.clear();

	labelman->clear();

	ordinary->clear();
	creatures->clear();

	collisionman.clear();

	landnav.cleanup();
	seanav.cleanup();
}

void Campaign::teardown()
{
	skybox.teardown();

	collisionman.clear();
}

void Campaign::save(const std::string &filepath)
{
	navigation_mesh_record navmesh_land;
	navigation_mesh_record navmesh_sea;

	// save the campaign navigation data
	{
		navmesh_land.tilemeshes.clear();

		const dtNavMesh *navmesh = landnav.get_navmesh();
		if (navmesh) {
			const dtNavMeshParams *navparams = navmesh->getParams();
			navmesh_land.origin = { navparams->orig[0], navparams->orig[1], navparams->orig[2] };
			navmesh_land.tilewidth = navparams->tileWidth;
			navmesh_land.tileheight = navparams->tileHeight;
			navmesh_land.maxtiles = navparams->maxTiles;
			navmesh_land.maxpolys = navparams->maxPolys;
			for (int i = 0; i < navmesh->getMaxTiles(); i++) {
				const dtMeshTile *tile = navmesh->getTile(i);
				if (!tile) { continue; }
				if (!tile->header) { continue; }
				const dtMeshHeader *tileheader = tile->header;
				struct navigation_tile_record navrecord;
				navrecord.x = tileheader->x;
				navrecord.y = tileheader->y;
				navrecord.data.insert(navrecord.data.end(), tile->data, tile->data + tile->dataSize);
				navmesh_land.tilemeshes.push_back(navrecord);
			}
		}
	}

	{
		navmesh_sea.tilemeshes.clear();

		const dtNavMesh *navmesh = seanav.get_navmesh();
		if (navmesh) {
			const dtNavMeshParams *navparams = navmesh->getParams();
			navmesh_sea.origin = { navparams->orig[0], navparams->orig[1], navparams->orig[2] };
			navmesh_sea.tilewidth = navparams->tileWidth;
			navmesh_sea.tileheight = navparams->tileHeight;
			navmesh_sea.maxtiles = navparams->maxTiles;
			navmesh_sea.maxpolys = navparams->maxPolys;
			for (int i = 0; i < navmesh->getMaxTiles(); i++) {
				const dtMeshTile *tile = navmesh->getTile(i);
				if (!tile) { continue; }
				if (!tile->header) { continue; }
				const dtMeshHeader *tileheader = tile->header;
				struct navigation_tile_record navrecord;
				navrecord.x = tileheader->x;
				navrecord.y = tileheader->y;
				navrecord.data.insert(navrecord.data.end(), tile->data, tile->data + tile->dataSize);
				navmesh_sea.tilemeshes.push_back(navrecord);
			}
		}
	}

	std::ofstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryOutputArchive archive(stream);
		archive(atlas, seed, navmesh_land, navmesh_sea, settlements, player_army, camera.position, camera.direction);
	} else {
		LOG(ERROR, "Save") << "save file " + filepath + "could not be saved";
	}
}

void Campaign::load(const std::string &filepath)
{
	navigation_mesh_record navmesh_land;
	navigation_mesh_record navmesh_sea;

	auto worldgraph = atlas.get_worldgraph();

	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(atlas, seed, navmesh_land, navmesh_sea, settlements, player_army, camera.position, camera.direction);
	} else {
		LOG(ERROR, "Save") << "save file " + filepath + " could not be loaded";
		return;
	}

	// load campaign the navigation data
	landnav.alloc(navmesh_land.origin, navmesh_land.tilewidth, navmesh_land.tileheight, navmesh_land.maxtiles, navmesh_land.maxpolys);
	for (const auto &tilemesh : navmesh_land.tilemeshes) {
		landnav.load_tilemesh(tilemesh.x, tilemesh.y, tilemesh.data);
	}
	seanav.alloc(navmesh_sea.origin, navmesh_sea.tilewidth, navmesh_sea.tileheight, navmesh_sea.maxtiles, navmesh_sea.maxpolys);
	for (const auto &tilemesh : navmesh_sea.tilemeshes) {
		seanav.load_tilemesh(tilemesh.x, tilemesh.y, tilemesh.data);
	}

	worldgraph->reload_references();
}
	
void Campaign::collide_camera()
{
	auto camresult = collisionman.cast_ray(glm::vec3(camera.position.x, atlas.SCALE.y, camera.position.z), glm::vec3(camera.position.x, 0.f, camera.position.z), physics::COLLISION_GROUP_HEIGHTMAP);

	if (camresult.hit) {
		float yoffset = camresult.point.y + 10.f;
		if (camera.position.y < yoffset) {
			camera.position.y = yoffset;
		}
	}
}
	
void Campaign::update_camera(const util::Input *input, const util::Window *window, float sensitivity, float delta)
{
	glm::vec2 rel_mousecoords = sensitivity * input->rel_mousecoords();
	camera.target(rel_mousecoords);

	float zoomlevel = camera.position.y / atlas.SCALE.y;
	zoomlevel = glm::clamp(zoomlevel, 0.f, 2.f);
	float modifier = 200.f * delta * (zoomlevel*zoomlevel);

	// camera look 
	if (input->mouse_grabbed() == false) {
		glm::vec2 abs_mousecoords = input->abs_mousecoords() / glm::vec2(float(window->width), float(window->height));
		if (abs_mousecoords.x > 0.99f) {
			camera.target(glm::vec2(30.f, 0.f));
		} else if (abs_mousecoords.x < 0.01f) {
			camera.target(glm::vec2(-30.f, 0.f));
		} else if (abs_mousecoords.y > 0.99f) { // horizontal look overrides vertical
			camera.target(glm::vec2(0.f, 20.f));
		} else if (abs_mousecoords.y < 0.01f) {
			camera.target(glm::vec2(0.f, -20.f));
		}
	}

	if (input->key_down(SDLK_w)) { 
		glm::vec2 dir = glm::normalize(glm::vec2(camera.direction.x, camera.direction.z));
		camera.position += modifier * glm::vec3(dir.x, 0.f, dir.y);
	}
	if (input->key_down(SDLK_s)) { 
		glm::vec2 dir = glm::normalize(glm::vec2(camera.direction.x, camera.direction.z));
		camera.position -= modifier * glm::vec3(dir.x, 0.f, dir.y);
	}
	if (input->key_down(SDLK_d)) { camera.move_right(modifier); }
	if (input->key_down(SDLK_a)) { camera.move_left(modifier); }

	// scroll camera forward or backward
	// smooth scroll
	int mousewheel = input->mousewheel_y();

	if (mousewheel > 0) {
		m_scroll_status = campaign_scroll_status::FORWARD;
		m_scroll_time = 1.f;
		m_scroll_speed += 0.5f;
	}
	if (mousewheel < 0) {
		m_scroll_status = campaign_scroll_status::BACKWARD;
		m_scroll_time = 1.f;
		m_scroll_speed += 0.5f;
	}
				
	m_scroll_speed = glm::clamp(m_scroll_speed, 1.f, 2.f);

	// reset
	if (m_scroll_status != campaign_scroll_status::NONE) {
		float pitch = glm::mix(-1.55f, -0.5f, 1.f - (camera.position.y / (4.f * atlas.SCALE.y)));
		m_scroll_time -= 5.f * delta;

		if (m_scroll_status == campaign_scroll_status::BACKWARD) {
			camera.position.y += m_scroll_speed * modifier;
		} else {
			camera.position.y -= m_scroll_speed * modifier;
		}

		if (m_scroll_time < 0.f) {
			m_scroll_status = campaign_scroll_status::NONE;
			m_scroll_time = 0.f;
			m_scroll_speed = 1.f;
		}

		camera.pitch = glm::clamp(pitch, -1.55f, -0.1f);
		camera.angles_to_direction();
	}

	camera.update();

	collide_camera();
}
	
void Campaign::update_labels()
{
	// scale between 1 and 10
	float label_scale = camera.position.y / atlas.SCALE.y;
	label_scale = glm::smoothstep(0.5f, 2.f, label_scale);
	label_scale = glm::clamp(10.f*label_scale, 1.f, 10.f);

	labelman->set_scale(label_scale);
	labelman->set_depth(label_scale > 3.f);
}
	
void Campaign::offset_entities()
{
	// movable entities vertical offset
	glm::vec3 origin = { player->position.x, atlas.SCALE.y, player->position.z };
	glm::vec3 end = { player->position.x, 0.f, player->position.z };
	auto result = collisionman.cast_ray(origin, end, physics::COLLISION_GROUP_HEIGHTMAP);
	player->set_y_offset(result.point.y);
}
	
void Campaign::update_faction_map()
{
	const float ratio = camera.position.y / atlas.SCALE.y;
	factions_visible = (ratio > 2.f) && show_factions;
	// map faction mode
	float colormix = 0.f;
	if (factions_visible) {
		colormix = glm::smoothstep(0.5f, 2.f, ratio);
		if (colormix < 0.25f) {
			colormix = 0.f;
		}
	}

	worldmap->set_faction_factor(colormix);
}
	
void Campaign::change_player_target(const glm::vec3 &ray)
{
	auto result = collisionman.cast_ray(camera.position, camera.position + (1000.f * ray), physics::COLLISION_GROUP_HEIGHTMAP | physics::COLLISION_GROUP_GHOSTS);
	if (result.hit) {
		player->set_target_type(TARGET_LAND);
		marker.position = result.point;
		if (result.object) {
			SettlementNode *node = static_cast<SettlementNode*>(result.object->getUserPointer());
			if (node) {
				player->set_target_type(TARGET_SETTLEMENT);
				battle_info.settlementID = node->get_tileref();
				battle_info.tileID = node->get_tileref();
			}
		}
		// get tile
		glm::vec2 position = geom::translate_3D_to_2D(result.point);
		const auto tily = atlas.tile_at_position(position);
		if (tily != nullptr && glm::distance(position, geom::translate_3D_to_2D(player->position)) < 10.f) {
			// embark or disembark
			if (player->get_movement_mode() == MOVEMENT_LAND && tily->land == false) {
				player->set_movement_mode(MOVEMENT_SEA);
				player->teleport(position);
			} else if (player->get_movement_mode() == MOVEMENT_SEA && tily->land == true) {
				player->set_movement_mode(MOVEMENT_LAND);
				player->teleport(position);
			}
		}
		// change path if found
		std::list<glm::vec2> waypoints;
		if (player->get_movement_mode() == MOVEMENT_LAND) {
			landnav.find_2D_path(geom::translate_3D_to_2D(player->position), geom::translate_3D_to_2D(marker.position), waypoints);
			player->set_path(waypoints);
		} else if (player->get_movement_mode() == MOVEMENT_SEA) {
			seanav.find_2D_path(geom::translate_3D_to_2D(player->position), geom::translate_3D_to_2D(marker.position), waypoints);
			player->set_path(waypoints);
		}
	}
}
	
