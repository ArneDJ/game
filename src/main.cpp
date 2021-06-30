#include <memory>
#include <iostream>
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

#include "util/geom.h"
#include "util/image.h"
#include "util/entity.h"
#include "util/camera.h"
#include "util/window.h"
#include "util/input.h"
#include "util/timer.h"
#include "util/animation.h"
#include "util/navigation.h"
#include "util/voronoi.h"
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
#include "graphics/worldmap.h"
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
#include "geography/terragen.h"
#include "geography/worldgraph.h"
#include "geography/mapfield.h"
#include "geography/atlas.h"
#include "geography/sitegen.h"
#include "geography/landscape.h"
#include "save.h"
#include "army.h"
//#include "util/sound.h" // TODO replace SDL_Mixer with OpenAL

enum class game_state {
	TITLE,
	NEW_CAMPAIGN,
	LOAD_CAMPAIGN,
	CAMPAIGN,
	BATTLE,
	EXIT
};

struct game_settings_t {
	std::string module_name;
	uint16_t window_width;
	uint16_t window_height;
	bool fullscreen;
	int FOV;
	float look_sensitivity;
	// graphics
	bool clouds_enabled;
};

struct shader_group_t {
	GRAPHICS::Shader object;
	GRAPHICS::Shader creature;
	GRAPHICS::Shader debug;
	GRAPHICS::Shader tree;
	GRAPHICS::Shader billboard;
	GRAPHICS::Shader font;
	GRAPHICS::Shader depth;
	GRAPHICS::Shader copy;
};

class Campaign {
public:
	long seed;
	UTIL::Navigation landnav;
	UTIL::Navigation seanav;
	UTIL::Camera camera;
	PHYSICS::PhysicsManager collisionman;
	std::unique_ptr<Atlas> atlas;
	// graphics
	std::unique_ptr<GRAPHICS::Worldmap> worldmap;
	std::unique_ptr<GRAPHICS::LabelManager> labelman;
	std::unique_ptr<GRAPHICS::RenderGroup> ordinary;
	std::unique_ptr<GRAPHICS::RenderGroup> creatures;
	GRAPHICS::Skybox skybox;
	// entities
	Entity marker;
	std::unique_ptr<Army> player;
	std::vector<SettlementNode*> settlements;
	std::vector<Entity*> entities;
	bool show_factions = false;
public:
	void init(const UTIL::Window *window, const struct shader_group_t *shaders);
	void load_assets();
	void add_armies();
	void add_trees();
	void add_settlements();
	void cleanup();
	void teardown();
public:
	void update_camera(const UTIL::Input *input, float sensitivity, float delta);
	void update_labels();
	void update_faction_map();
	void offset_entities();
	void change_player_target(const glm::vec3 &ray);
private:
	void collide_camera();
};
	
void Campaign::init(const UTIL::Window *window, const struct shader_group_t *shaders)
{
	atlas = std::make_unique<Atlas>();

	const auto terragen = atlas->get_terragen();
	worldmap = std::make_unique<GRAPHICS::Worldmap>(atlas->SCALE, &terragen->heightmap, atlas->get_watermap(), &terragen->rainmap, atlas->get_materialmasks(), atlas->get_factions());

	ordinary = std::make_unique<GRAPHICS::RenderGroup>(&shaders->debug);
	creatures = std::make_unique<GRAPHICS::RenderGroup>(&shaders->object);

	glm::vec2 startpos = { 2010.f, 2010.f };
	player = std::make_unique<Army>(startpos, 40.f);
	
	skybox.init(window->width, window->height);
	
	labelman = std::make_unique<GRAPHICS::LabelManager>("fonts/diablo.ttf", 30);

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
	
void Campaign::add_armies()
{
	player->teleport(glm::vec2(2010.f, 2010.f));

	std::vector<const Entity*> ents;
	ents.push_back(player.get());

	creatures->add_object(MediaManager::load_model("duck.glb"), ents);
}

void Campaign::add_trees()
{
	auto trees = atlas->get_trees();

	std::vector<const Entity*> ents;

	for (int i = 0; i < trees.size(); i++) {
		Entity *entity = new Entity(trees[i].position, trees[i].rotation);
		entity->scale = trees[i].scale;
		entities.push_back(entity);
		ents.push_back(entity);
	}

	creatures->add_object(MediaManager::load_model("map/pines.glb"), ents);
}

void Campaign::add_settlements()
{
	btCollisionShape *shape = new btSphereShape(5.f);
	collisionman.add_shape(shape);

	for (const auto &tile : atlas->get_worldgraph()->tiles) {
		if (tile.site == CASTLE || tile.site == TOWN) {
			glm::vec3 position = { tile.center.x, 0.f, tile.center.y };
			glm::vec3 origin = { tile.center.x, atlas->SCALE.y, tile.center.y };
			auto result = collisionman.cast_ray(origin, position, PHYSICS::COLLISION_GROUP_HEIGHTMAP);
			position.y = result.point.y;
			SettlementNode *node = new SettlementNode { position, glm::quat(1.f, 0.f, 0.f, 0.f), shape, tile.index };
			settlements.push_back(node);
			collisionman.add_ghost_object(node->ghost_object, PHYSICS::COLLISION_GROUP_GHOSTS, PHYSICS::COLLISION_GROUP_GHOSTS);
		}
	}

	std::vector<const Entity*> ents;

	for (int i = 0; i < settlements.size(); i++) {
		ents.push_back(settlements[i]);
	}

	ordinary->add_object(MediaManager::load_model("map/fort.glb"), ents);

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
	NameGen::Generator towngen(pattern.c_str());
	for (const auto &ent : settlements) {
		labelman->add(towngen.toString(), glm::vec3(1.f), ent->position + glm::vec3(0.f, 8.f, 0.f));
	}
}
	
void Campaign::cleanup()
{
	for (int i = 0; i < entities.size(); i++) {
		delete entities[i];
	}
	entities.clear();

	for (int i = 0; i < settlements.size(); i++) {
		delete settlements[i];
	}
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
	
void Campaign::collide_camera()
{
	auto camresult = collisionman.cast_ray(glm::vec3(camera.position.x, atlas->SCALE.y, camera.position.z), glm::vec3(camera.position.x, 0.f, camera.position.z), PHYSICS::COLLISION_GROUP_HEIGHTMAP);

	if (camresult.hit) {
		float yoffset = camresult.point.y + 10.f;
		if (camera.position.y < yoffset) {
			camera.position.y = yoffset;
		}
	}
}
	
void Campaign::update_camera(const UTIL::Input *input, float sensitivity, float delta)
{
	glm::vec2 rel_mousecoords = sensitivity * input->rel_mousecoords();
	camera.target(rel_mousecoords);

	float zoomlevel = camera.position.y / atlas->SCALE.y;
	zoomlevel = glm::clamp(zoomlevel, 0.f, 2.f);
	float modifier = 200.f * delta * (zoomlevel*zoomlevel);

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
	camera.update();
	int mousewheel = input->mousewheel_y();
	if (mousewheel > 0) {
		camera.move_forward(10.0*mousewheel*modifier);
	}
	if (mousewheel < 0) {
		camera.move_backward(10.0*abs(mousewheel)*modifier);
	}
		
	collide_camera();
}
	
void Campaign::update_labels()
{
	// scale between 1 and 10
	float label_scale = camera.position.y / atlas->SCALE.y;
	label_scale = glm::smoothstep(0.5f, 2.f, label_scale);
	label_scale = glm::clamp(10.f*label_scale, 1.f, 10.f);

	labelman->set_scale(label_scale);
	labelman->set_depth(label_scale > 3.f);
}
	
void Campaign::offset_entities()
{
	// movable entities vertical offset
	glm::vec3 origin = { player->position.x, atlas->SCALE.y, player->position.z };
	glm::vec3 end = { player->position.x, 0.f, player->position.z };
	auto result = collisionman.cast_ray(origin, end, PHYSICS::COLLISION_GROUP_HEIGHTMAP);
	player->set_y_offset(result.point.y);
}
	
void Campaign::update_faction_map()
{
	// map faction mode
	float colormix = 0.f;
	if (show_factions) {
		colormix = camera.position.y / atlas->SCALE.y;
		colormix = glm::smoothstep(0.5f, 2.f, colormix);
		if (colormix < 0.25f) {
			colormix = 0.f;
		}
	}

	worldmap->set_faction_factor(colormix);
}
	
void Campaign::change_player_target(const glm::vec3 &ray)
{
	auto result = collisionman.cast_ray(camera.position, camera.position + (1000.f * ray), PHYSICS::COLLISION_GROUP_HEIGHTMAP | PHYSICS::COLLISION_GROUP_GHOSTS);
	if (result.hit) {
		player->set_target_type(TARGET_LAND);
		marker.position = result.point;
		if (result.object) {
			SettlementNode *node = static_cast<SettlementNode*>(result.object->getUserPointer());
			if (node) {
				player->set_target_type(TARGET_SETTLEMENT);
			}
		}
		// get tile
		glm::vec2 position = geom::translate_3D_to_2D(result.point);
		const struct tile *tily = atlas->tile_at_position(position);
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
	
class Battle {
public:
	bool naval = false;
	UTIL::Camera camera;
	PHYSICS::PhysicsManager physicsman;
	std::unique_ptr<Landscape> landscape;
	// graphics
	std::unique_ptr<GRAPHICS::RenderGroup> ordinary;
	std::unique_ptr<GRAPHICS::RenderGroup> creatures;
	std::unique_ptr<GRAPHICS::Terrain> terrain;
	std::unique_ptr<GRAPHICS::Forest> forest;
	GRAPHICS::Skybox skybox;
	// entities
	Creature *player;
	std::vector<StationaryObject*> stationaries;
	std::vector<StationaryObject*> tree_stationaries;
	std::vector<Entity> entities;

public:
	void init(const MODULE::Module *mod, const UTIL::Window *window, const struct shader_group_t *shaders);
	void load_assets(const MODULE::Module *mod);
	void add_creatures(const MODULE::Module *mod);
	void add_buildings();
	void add_trees();
	void cleanup();
	void teardown();
};
	
void Battle::init(const MODULE::Module *mod, const UTIL::Window *window, const struct shader_group_t *shaders)
{
	landscape = std::make_unique<Landscape>(mod, 2048);

	terrain = std::make_unique<GRAPHICS::Terrain>(landscape->SCALE, landscape->get_heightmap(), landscape->get_normalmap(), landscape->get_sitemasks());

	load_assets(mod);

	ordinary = std::make_unique<GRAPHICS::RenderGroup>(&shaders->debug);
	creatures = std::make_unique<GRAPHICS::RenderGroup>(&shaders->creature);

	skybox.init(window->width, window->height);
	
	forest = std::make_unique<GRAPHICS::Forest>(&shaders->tree, &shaders->billboard);
}

void Battle::load_assets(const MODULE::Module *mod)
{
	landscape->load_buildings();
	
	terrain->insert_material("STONEMAP", MediaManager::load_texture("ground/stone.dds"));
	terrain->insert_material("REGOLITH_MAP", MediaManager::load_texture("ground/grass.dds"));
	terrain->insert_material("GRAVELMAP", MediaManager::load_texture("ground/gravel.dds"));
	terrain->insert_material("DETAILMAP", MediaManager::load_texture("ground/stone_normal.dds"));
	terrain->insert_material("WAVE_BUMPMAP", MediaManager::load_texture("ground/water_normal.dds"));
}
	
void Battle::add_creatures(const MODULE::Module *mod)
{
	glm::vec3 end = glm::vec3(3072.f, 0.f, 3072.f);
	glm::vec3 origin = { end.x, landscape->SCALE.y, end.z };
	auto result = physicsman.cast_ray(origin, end, PHYSICS::COLLISION_GROUP_HEIGHTMAP | PHYSICS::COLLISION_GROUP_WORLD);
	if (result.hit) {
		origin = result.point;
		origin.y += 1.f;
	}
	player = new Creature { origin, glm::quat(1.f, 0.f, 0.f, 0.f), MediaManager::load_model("human.glb"), mod->test_armature };

	physicsman.add_body(player->get_body(), PHYSICS::COLLISION_GROUP_ACTOR, PHYSICS::COLLISION_GROUP_ACTOR | PHYSICS::COLLISION_GROUP_WORLD | PHYSICS::COLLISION_GROUP_HEIGHTMAP);

	std::vector<const Entity*> ents;
	ents.push_back(player);
	creatures->add_object(MediaManager::load_model("human.glb"), ents);
}
	
void Battle::add_buildings()
{
	// add houses
	const auto houses = landscape->get_houses();
	for (const auto &house : houses) {
		std::vector<const Entity*> house_entities;
		const GRAPHICS::Model *model = house.model;
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
	// add stationary objects to physics
	for (const auto &stationary : stationaries) {
		physicsman.add_body(stationary->body, PHYSICS::COLLISION_GROUP_WORLD, PHYSICS::COLLISION_GROUP_ACTOR | PHYSICS::COLLISION_GROUP_RAY | PHYSICS::COLLISION_GROUP_RAGDOLL);
	}
}
	
void Battle::add_trees()
{
	const auto trees = landscape->get_trees();

	entities.clear();

	for (const auto &tree : trees) {
		const GRAPHICS::Model *trunk = MediaManager::load_model(tree.trunk);
		const GRAPHICS::Model *leaves = MediaManager::load_model(tree.leaves);
		const GRAPHICS::Model *billboard = MediaManager::load_model(tree.billboard);
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

	for (const auto &stationary : tree_stationaries) {
		physicsman.add_body(stationary->body, PHYSICS::COLLISION_GROUP_WORLD, PHYSICS::COLLISION_GROUP_ACTOR | PHYSICS::COLLISION_GROUP_RAY | PHYSICS::COLLISION_GROUP_RAGDOLL);
	}
}

void Battle::cleanup()
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
	
class Game {
public:
	bool init();
	void run();
	void shutdown();
private:
	bool running;
	bool debugmode;
	MODULE::Module modular;
	enum game_state state;
	struct game_settings_t settings;
	Saver saver;
	UTIL::Window window;
	UTIL::Input input;
	UTIL::Timer timer;
	GRAPHICS::TextManager *textman;
	Debugger debugger;
	Campaign campaign;
	Battle battle;
	// graphics
	GRAPHICS::RenderManager renderman;
	struct shader_group_t shaders;
	float fog_factor = 0.0005f; // TODO atmosphere
	glm::vec3 ambiance_color = { 1.f, 1.f, 1.f };
private:
	void load_settings();
	void set_opengl_states();
	void load_shaders();
	void load_module();
private:
	void prepare_campaign();
	void run_campaign();
	void update_campaign();
	void new_campaign();
	void load_campaign();
private:
	void prepare_battle();
	void run_battle();
	void update_battle();
};

void Game::set_opengl_states()
{
	// set OpenGL states
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glLineWidth(5.f);
}

void Game::load_settings()
{
	static const std::string INI_SETTINGS_PATH = "settings.ini";
	INIReader reader = { INI_SETTINGS_PATH.c_str() };
	if (reader.ParseError() != 0) { 
		LOG(ERROR, "Settings") << "Could not load ini file: " + INI_SETTINGS_PATH;
	}
	settings.window_width = reader.GetInteger("", "WINDOW_WIDTH", 1920);
	settings.window_height = reader.GetInteger("", "WINDOW_HEIGHT", 1080);
	settings.fullscreen = reader.GetBoolean("", "FULLSCREEN", false);
	settings.FOV = reader.GetInteger("", "FOV", 90);
	settings.look_sensitivity = float(reader.GetInteger("", "MOUSE_SENSITIVITY", 1));
	debugmode = reader.GetBoolean("", "DEBUG_MODE", false);
	
	// graphics settings
	settings.clouds_enabled = reader.GetBoolean("", "CLOUDS_ENABLED", false);

	// module name
	settings.module_name = reader.Get("", "MODULE", "native");
}

bool Game::init()
{
	// initialize the logger
	auto sink_cout = std::make_shared<AixLog::SinkCout>(AixLog::Severity::trace);
	auto sink_file = std::make_shared<AixLog::SinkFile>(AixLog::Severity::trace, "error.log");
	AixLog::Log::init({sink_cout, sink_file});

	// load settings
	load_settings();

	// initialize window
	if (!window.open(settings.window_width, settings.window_height)) {
		return false;
	}

	if (settings.fullscreen) {
		window.set_fullscreen();
	}
	
	SDL_SetRelativeMouseMode(SDL_FALSE);

	set_opengl_states();

	renderman.init(settings.window_width, settings.window_height);

	if (debugmode) {
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		(void)io;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer bindings
		ImGui_ImplSDL2_InitForOpenGL(window.window, window.glcontext);
		ImGui_ImplOpenGL3_Init("#version 430");
	}

	// find the save directory on the system
	char *savepath = SDL_GetPrefPath("archeon", "saves");
	if (savepath) {
		saver.change_directory(savepath);
		SDL_free(savepath);
	} else {
		LOG(ERROR, "Save") << "could not find user pref path";
		return false;
	}

	textman = new GRAPHICS::TextManager { "fonts/exocet.ttf", 40 };

	load_shaders();

	if (debugmode) {
		debugger.init(&shaders.debug);
	}

	return true;
}
	
void Game::load_shaders()
{
	shaders.debug.compile("shaders/debug.vert", GL_VERTEX_SHADER);
	shaders.debug.compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	shaders.debug.link();

	shaders.object.compile("shaders/object.vert", GL_VERTEX_SHADER);
	shaders.object.compile("shaders/object.frag", GL_FRAGMENT_SHADER);
	shaders.object.link();

	shaders.creature.compile("shaders/creature.vert", GL_VERTEX_SHADER);
	shaders.creature.compile("shaders/creature.frag", GL_FRAGMENT_SHADER);
	shaders.creature.link();

	shaders.billboard.compile("shaders/billboard.vert", GL_VERTEX_SHADER);
	shaders.billboard.compile("shaders/billboard.frag", GL_FRAGMENT_SHADER);
	shaders.billboard.link();

	shaders.tree.compile("shaders/tree.vert", GL_VERTEX_SHADER);
	shaders.tree.compile("shaders/tree.frag", GL_FRAGMENT_SHADER);
	shaders.tree.link();

	shaders.font.compile("shaders/font.vert", GL_VERTEX_SHADER);
	shaders.font.compile("shaders/font.frag", GL_FRAGMENT_SHADER);
	shaders.font.link();

	shaders.depth.compile("shaders/depthmap.vert", GL_VERTEX_SHADER);
	shaders.depth.compile("shaders/depthmap.frag", GL_FRAGMENT_SHADER);
	shaders.depth.link();

	shaders.copy.compile("shaders/copy.comp", GL_COMPUTE_SHADER);
	shaders.copy.link();
}
	
void Game::load_module()
{
	modular.load(settings.module_name);

	MediaManager::change_path(modular.path);
}

void Game::shutdown()
{
	campaign.teardown();

	battle.teardown();

	MediaManager::teardown();

	if (debugmode) {
		debugger.teardown();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
	}

	delete textman;

	renderman.teardown();

	window.close();
}

void Game::update_battle()
{
	// input
	input.update();
	if (input.exit_request()) {
		state = game_state::EXIT;
	}

	glm::vec2 rel_mousecoords = settings.look_sensitivity * input.rel_mousecoords();
	battle.camera.target(rel_mousecoords);

	float modifier = 20.f * timer.delta;
	if (input.key_down(SDLK_w)) { battle.camera.move_forward(modifier); }
	if (input.key_down(SDLK_s)) { battle.camera.move_backward(modifier); }
	if (input.key_down(SDLK_d)) { battle.camera.move_right(modifier); }
	if (input.key_down(SDLK_a)) { battle.camera.move_left(modifier); }

	if (debugmode) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(window.window);
		ImGui::NewFrame();
		ImGui::Begin("Battle Debug Mode");
		ImGui::SetWindowSize(ImVec2(400, 200));
		ImGui::Text("ms per frame: %d", timer.ms_per_frame);
		ImGui::Text("cam position: %f, %f, %f", battle.camera.position.x, battle.camera.position.y, battle.camera.position.z);
		ImGui::Text("anim mix: %f", battle.player->m_animation_mix);
		if (ImGui::Button("Ragdoll mode")) { 
			battle.player->m_ragdoll_mode = !battle.player->m_ragdoll_mode; 
			if (battle.player->m_ragdoll_mode) {
				battle.player->add_ragdoll(battle.physicsman.get_world());
			} else {
				battle.player->remove_ragdoll(battle.physicsman.get_world());
			}
		}
		if (ImGui::Button("Exit Battle")) { state = game_state::CAMPAIGN; }
		ImGui::End();
	}

	battle.player->move(battle.camera.direction, input.key_down(SDLK_w), input.key_down(SDLK_s), input.key_down(SDLK_d), input.key_down(SDLK_a));
	if (input.key_down(SDLK_SPACE)) {
		battle.player->jump();
	}

	input.update_keymap();

	// update creatures
	battle.player->update(battle.physicsman.get_world());

	battle.physicsman.update(timer.delta);

	battle.player->sync(timer.delta);

	battle.camera.translate(battle.player->position - (5.f * battle.camera.direction));
	battle.camera.update();
	
	// update atmosphere
	battle.skybox.colorize(modular.atmosphere.day.zenith, modular.atmosphere.day.horizon, glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)), ambiance_color, settings.clouds_enabled);
	battle.skybox.update(&battle.camera, timer.elapsed);
}
	
void Game::prepare_battle()
{
	battle.camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	battle.camera.project();

	// find the campaign tile the player is on and prepare local scene properties based on it
	glm::vec2 position = geom::translate_3D_to_2D(campaign.player->position);
	const struct tile *tily = campaign.atlas->tile_at_position(position);
	uint32_t tileref = 0;
	float amp = 0.f;
	uint8_t precipitation = 0;
	uint8_t tree_density = 0;
	uint8_t temperature = 0;
	int32_t local_seed = 0;
	uint8_t site_radius = 0;
	enum tile_regolith regolith = tile_regolith::SAND;
	enum tile_feature feature = tile_feature::NONE;
	if (tily) {
		battle.naval = !tily->land;
		amp = tily->amp;
		tileref = tily->index;
		precipitation = tily->precipitation;
		temperature = tily->temperature;
		regolith = tily->regolith;
		feature = tily->feature;
		std::mt19937 gen(campaign.seed);
		gen.discard(tileref);
		std::uniform_int_distribution<int32_t> local_seed_distrib;
		local_seed = local_seed_distrib(gen);
		if (campaign.player->get_target_type() == TARGET_SETTLEMENT) {
			switch (tily->site) {
			case CASTLE: site_radius = 1; break;
			case TOWN: site_radius = 2; break;
			}
		}
	}

	glm::vec3 grasscolor = glm::mix(glm::vec3(1.5f)*modular.palette.grass.min, modular.palette.grass.max, precipitation / 255.f);

	if (feature == tile_feature::WOODS) {
		tree_density = 255;
	} else if (regolith == tile_regolith::SNOW || regolith == tile_regolith::SAND) {
		tree_density = 0;
	} else {
		tree_density = (precipitation > 64) ? 64 : precipitation;
	}

	battle.landscape->generate(campaign.seed, tileref, local_seed, amp, precipitation, temperature, tree_density, site_radius, false, battle.naval);
	battle.terrain->reload(battle.landscape->get_heightmap(), battle.landscape->get_normalmap(), battle.landscape->get_sitemasks());
	battle.terrain->change_atmosphere(glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)), modular.atmosphere.day.horizon, fog_factor, ambiance_color);
	
	glm::vec3 rock_color = glm::mix(glm::vec3(0.8f), glm::vec3(1.f), temperature / 255.f);
	
	bool grass_present = false;
	// main terrain soil
	if (regolith == tile_regolith::GRASS) {
		grass_present = true;
		battle.terrain->insert_material("REGOLITH_MAP", MediaManager::load_texture("ground/grass.dds"));
	} else if (regolith == tile_regolith::SAND) {
		battle.terrain->insert_material("REGOLITH_MAP", MediaManager::load_texture("ground/sand.dds"));
		grasscolor = { 0.96, 0.83, 0.63 };
		rock_color = glm::vec3(0.96, 0.83, 0.63);
	} else {
		battle.terrain->insert_material("REGOLITH_MAP", MediaManager::load_texture("ground/snow.dds"));
		grasscolor = { 1.f, 1.f, 1.f };
	}

	battle.terrain->change_grass(grasscolor, grass_present);
	battle.terrain->change_rock_color(rock_color);

	battle.physicsman.add_heightfield(battle.landscape->get_heightmap(), battle.landscape->SCALE, PHYSICS::COLLISION_GROUP_HEIGHTMAP, PHYSICS::COLLISION_GROUP_ACTOR | PHYSICS::COLLISION_GROUP_RAY | PHYSICS::COLLISION_GROUP_RAGDOLL);

	// add entities
	battle.add_trees();
	battle.add_buildings();
	battle.add_creatures(&modular);

	// visualize BVH leaves
	//int next = 0;
	std::mt19937 gen(1337);
	std::uniform_real_distribution<float> color_dist(0.f, 1.f);

	/*
	for (const auto &leaf : battle.forest->m_bvh.leafs) {
		glm::vec3 color = { color_dist(gen), color_dist(gen), color_dist(gen) };
		debugger.add_cube_mesh(leaf->bounds.min, leaf->bounds.max, color);
		//battle.camera.position = leaf->bounds.max;
	}
	*/

	battle.skybox.prepare();

	battle.forest->set_atmosphere(glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)), modular.atmosphere.day.horizon, fog_factor, ambiance_color);

	if (battle.naval) {
		battle.camera.position = { 3072.f, 270.f, 3072.f };
	} else {
		battle.camera.position = { 3072.f, 200.f, 3072.f };
	}
	battle.camera.lookat(glm::vec3(0.f, 0.f, 0.f));
}

void Game::run_battle()
{
	prepare_battle();

	while (state == game_state::BATTLE) {
		timer.begin();

		update_battle();

		renderman.bind_FBO();
	
		battle.ordinary->display(&battle.camera);

		const auto instancebuf = battle.player->joints();
		instancebuf->bind(GL_TEXTURE20);
		shaders.creature.uniform_bool("RAGDOLL", battle.player->m_ragdoll_mode);
		battle.creatures->display(&battle.camera);

		if (debugmode) {
			debugger.render_bboxes(&battle.camera);
		}

		battle.forest->display(&battle.camera);

		battle.terrain->display_land(&battle.camera);

		battle.skybox.display(&battle.camera);
		
		battle.terrain->display_grass(&battle.camera);

		if (battle.naval) {
			renderman.bind_depthmap(GL_TEXTURE2);
			battle.terrain->display_water(&battle.camera, timer.elapsed);
		}

		renderman.final_render();

		if (debugmode) {
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		window.swap();
		timer.end();
	}
	
	if (debugmode) {
		debugger.delete_bboxes();
	}

	battle.cleanup();
}
	
void Game::update_campaign()
{
	input.update();
	if (input.exit_request()) {
		state = game_state::EXIT;
	}
	
	campaign.update_camera(&input, settings.look_sensitivity, timer.delta);

	if (input.key_pressed(SDL_BUTTON_RIGHT) == true && input.mouse_grabbed() == false) {
		glm::vec3 ray = campaign.camera.ndc_to_ray(input.abs_mousecoords());
		campaign.change_player_target(ray);
	}

	campaign.player->update(timer.delta);

	if (debugmode) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(window.window);
		ImGui::NewFrame();
		ImGui::Begin("Campaign Debug Mode");
		ImGui::SetWindowSize(ImVec2(400, 200));
		ImGui::Text("ms per frame: %d", timer.ms_per_frame);
		ImGui::Text("cam position: %f, %f, %f", campaign.camera.position.x, campaign.camera.position.y, campaign.camera.position.z);
		ImGui::Text("player position: %f, %f, %f", campaign.player->position.x, campaign.player->position.y, campaign.player->position.z);
		if (ImGui::Button("Show factions")) { campaign.show_factions = !campaign.show_factions; }
		if (ImGui::Button("Battle scene")) { state = game_state::BATTLE; }
		if (ImGui::Button("Title screen")) { state = game_state::TITLE; }
		if (ImGui::Button("Exit Game")) { state = game_state::EXIT; }
		ImGui::End();
	}

	// trigger battle
	if (campaign.player->get_target_type() == TARGET_SETTLEMENT) {
		if (glm::distance(geom::translate_3D_to_2D(campaign.player->position), geom::translate_3D_to_2D(campaign.marker.position)) < 1.f) {
			state = game_state::BATTLE;
		}
	}

	input.update_keymap();

	campaign.offset_entities();

	// update atmosphere
	campaign.skybox.colorize(modular.atmosphere.day.zenith, modular.atmosphere.day.horizon, glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)), ambiance_color, false);
	campaign.skybox.update(&campaign.camera, timer.elapsed);
	
	campaign.update_faction_map();

	campaign.update_labels();
}
	
void Game::new_campaign()
{
	// generate a new seed
	std::random_device rd;
	std::uniform_int_distribution<long> dis;
	std::mt19937 gen(rd());
	campaign.seed = dis(gen);
	//campaign.seed = 1337;
	//campaign.seed = 4998651408012010310;
	//campaign.seed = 8038877013446859113;
	//campaign.seed = 6900807170427947938;

	campaign.atlas->generate(campaign.seed, &modular.params);

	campaign.atlas->create_land_navigation();
	const auto land_navsoup = campaign.atlas->get_navsoup();
	campaign.landnav.build(land_navsoup.vertices, land_navsoup.indices);

	campaign.atlas->create_sea_navigation();
	const auto sea_navsoup = campaign.atlas->get_navsoup();
	campaign.seanav.build(sea_navsoup.vertices, sea_navsoup.indices);
	
	saver.save("game.save", campaign.atlas.get(), &campaign.landnav, &campaign.seanav, campaign.seed);
	
	prepare_campaign();
	run_campaign();
}
	
void Game::load_campaign()
{
	saver.load("game.save", campaign.atlas.get(), &campaign.landnav, &campaign.seanav, campaign.seed);

	prepare_campaign();
	run_campaign();
}

void Game::prepare_campaign()
{
	campaign.camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	campaign.camera.project();

	// campaign world map data
	campaign.atlas->create_mapdata(campaign.seed);

	const auto terragen = campaign.atlas->get_terragen();
	campaign.worldmap->reload(&terragen->heightmap, campaign.atlas->get_watermap(), &terragen->rainmap, campaign.atlas->get_materialmasks(), campaign.atlas->get_factions());
	campaign.worldmap->reload_temperature(&terragen->tempmap);
	campaign.worldmap->change_atmosphere(modular.atmosphere.day.horizon, 0.0002f, glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)));
	campaign.worldmap->change_groundcolors(modular.palette.grass.min, modular.palette.grass.max, modular.palette.rock_base.min, modular.palette.rock_base.max, modular.palette.rock_desert.min, modular.palette.rock_desert.max);

	campaign.collisionman.add_heightfield(&terragen->heightmap, campaign.atlas->SCALE, PHYSICS::COLLISION_GROUP_HEIGHTMAP, PHYSICS::COLLISION_GROUP_HEIGHTMAP | PHYSICS::COLLISION_GROUP_RAY);
	campaign.collisionman.add_heightfield(campaign.atlas->get_watermap(), campaign.atlas->SCALE, PHYSICS::COLLISION_GROUP_HEIGHTMAP, PHYSICS::COLLISION_GROUP_HEIGHTMAP | PHYSICS::COLLISION_GROUP_RAY);

	campaign.camera.position = { 2048.f, 200.f, 2048.f };
	campaign.camera.lookat(glm::vec3(0.f, 0.f, 0.f));

	shaders.billboard.use();
	shaders.billboard.uniform_float("FOG_FACTOR", 0.0002f);
	shaders.billboard.uniform_vec3("FOG_COLOR", modular.atmosphere.day.horizon);
	shaders.billboard.uniform_vec3("AMBIANCE_COLOR", glm::vec3(1.f));

	// add campaign entities
	campaign.marker.position = { 2010.f, 200.f, 2010.f };
	std::vector<const Entity*> ents;
	ents.push_back(&campaign.marker);
	campaign.ordinary->add_object(MediaManager::load_model("cone.glb"), ents);

	campaign.add_armies();

	campaign.add_trees();

	campaign.add_settlements();
}

void Game::run_campaign()
{
	state = game_state::CAMPAIGN;
	
	Entity dragon = Entity(glm::vec3(2048.f, 160.f, 2048.f), glm::quat(1.f, 0.f, 0.f, 0.f));
	std::vector<const Entity*> ents;
	ents.push_back(&dragon);
	campaign.ordinary->add_object(MediaManager::load_model("dragon.glb"), ents);

	while (state == game_state::CAMPAIGN) {
		timer.begin();

		update_campaign();

		renderman.bind_FBO();
	
		campaign.worldmap->display_land(&campaign.camera);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		renderman.bind_depthmap(GL_TEXTURE20); // TODO bind by name
		campaign.worldmap->display_water(&campaign.camera, timer.elapsed);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	//
		campaign.ordinary->display(&campaign.camera);

		shaders.object.use();
		shaders.object.uniform_float("FOG_FACTOR", 0.0002f);
		shaders.object.uniform_vec3("FOG_COLOR", modular.atmosphere.day.horizon);
		shaders.object.uniform_vec3("CAM_POS", campaign.camera.position);
		shaders.object.uniform_vec3("SUN_POS", glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)));
		campaign.creatures->display(&campaign.camera);

		campaign.skybox.display(&campaign.camera);
		
		campaign.labelman->display(&campaign.camera);

		renderman.final_render();

		if (debugmode) {
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		window.swap();
		timer.end();

		if (state == game_state::BATTLE) {
			run_battle();
			campaign.player->set_target_type(TARGET_NONE);
		}
	}

	if (debugmode) {
		debugger.delete_navmeshes();
	}
	
	campaign.cleanup();
}

void Game::run()
{
	// first import the game module
	load_module();

	// now that the module is loaded we initialize the campaign and battle
	campaign.init(&window, &shaders);
	battle.init(&modular, &window, &shaders);
	
	state = game_state::TITLE;
	
	textman->add_text("Hello World!", glm::vec3(1.f, 1.f, 1.f), glm::vec2(50.f, 400.f));
	textman->add_text("Archeon Prototype", glm::vec3(1.f, 1.f, 1.f), glm::vec2(50.f, 100.f));

	while (state == game_state::TITLE) {
		input.update();
		if (input.exit_request()) {
			state = game_state::EXIT;
		}

		if (debugmode) {
			// Start the Dear ImGui frame
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame(window.window);
			ImGui::NewFrame();
			ImGui::Begin("Main Menu Debug Mode");
			ImGui::SetWindowSize(ImVec2(400, 200));
			if (ImGui::Button("New World")) {
				state = game_state::NEW_CAMPAIGN;
			}
			if (ImGui::Button("Load World")) {
				state = game_state::LOAD_CAMPAIGN;
			}
			if (ImGui::Button("Exit")) { state = game_state::EXIT; }
			ImGui::End();
		}
		
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shaders.font.use();
		glm::mat4 menu_project = glm::ortho(0.0f, float(window.width), 0.0f, float(window.height));
		shaders.font.uniform_mat4("PROJECT", menu_project);
		textman->display();

		if (debugmode) {
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		window.swap();

		if (state == game_state::NEW_CAMPAIGN) {
			new_campaign();
		}
		if (state == game_state::LOAD_CAMPAIGN) {
			load_campaign();
		}
	}
}

int main(int argc, char *argv[])
{
	Game archeon;
	if (!archeon.init()) {
		return EXIT_FAILURE;
	}

	archeon.run();

	archeon.shutdown();

	return EXIT_SUCCESS;
}
