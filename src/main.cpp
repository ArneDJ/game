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

#include "core/geom.h"
#include "core/image.h"
#include "core/entity.h"
#include "core/camera.h"
#include "core/window.h"
#include "core/input.h"
#include "core/timer.h"
#include "core/animation.h"
#include "core/navigation.h"
#include "core/voronoi.h"
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
#include "physics/heightfield.h"
#include "physics/physics.h"
#include "media.h"
#include "object.h"
#include "creature.h"
#include "debugger.h"
#include "module.h"
#include "geography/terragen.h"
#include "geography/worldgraph.h"
#include "geography/mapfield.h"
#include "geography/atlas.h"
#include "geography/sitegen.h"
#include "geography/landscape.h"
#include "save.h"
#include "army.h"
//#include "core/sound.h" // TODO replace SDL_Mixer with OpenAL

enum game_state_t {
	GS_TITLE,
	GS_NEW_CAMPAIGN,
	GS_LOAD_CAMPAIGN,
	GS_CAMPAIGN,
	GS_BATTLE,
	GS_EXIT
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
	Shader object;
	Shader debug;
	Shader billboard;
	Shader font;
	Shader depth;
	Shader copy;
};

class Campaign {
public:
	long seed;
	Navigation landnav;
	Navigation seanav;
	CORE::Camera camera;
	PHYSICS::PhysicsManager collisionman;
	std::unique_ptr<Atlas> atlas;
	std::unique_ptr<Worldmap> worldmap;
	// graphics
	std::unique_ptr<LabelManager> labelman;
	std::unique_ptr<RenderGroup> ordinary;
	std::unique_ptr<RenderGroup> creatures;
	std::unique_ptr<BillboardGroup> billboards;
	Skybox skybox;
	// entities
	Entity marker;
	std::unique_ptr<Army> player;
	std::vector<SettlementNode*> settlements;
	std::vector<Entity*> entities;
	bool show_factions = false;
public:
	void init(const CORE::Window *window, const struct shader_group_t *shaders);
	void load_assets();
	void add_armies();
	void add_trees();
	void add_settlements();
	void cleanup();
	void teardown();
public:
	void update_camera(const CORE::Input *input, float sensitivity, float delta);
	void update_labels();
	void update_faction_map();
	void offset_entities();
	void change_player_target(const glm::vec3 &ray);
private:
	void collide_camera();
};
	
void Campaign::init(const CORE::Window *window, const struct shader_group_t *shaders)
{
	atlas = std::make_unique<Atlas>();

	worldmap = std::make_unique<Worldmap>(atlas->SCALE, atlas->get_heightmap(), atlas->get_watermap(), atlas->get_rainmap(), atlas->get_materialmasks(), atlas->get_factions());

	ordinary = std::make_unique<RenderGroup>(&shaders->debug);
	creatures = std::make_unique<RenderGroup>(&shaders->object);

	billboards = std::make_unique<BillboardGroup>(&shaders->billboard);

	glm::vec2 startpos = { 2010.f, 2010.f };
	player = std::make_unique<Army>(startpos, 20.f);
	
	skybox.init(window->width, window->height);
	
	labelman = std::make_unique<LabelManager>("fonts/diablo.ttf", 30);

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

	billboards->add_billboard(MediaManager::load_texture("trees/fir.dds"), ents);
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
	billboards->clear();

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
	
void Campaign::update_camera(const CORE::Input *input, float sensitivity, float delta)
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
		glm::vec2 position = translate_3D_to_2D(result.point);
		const struct tile *tily = atlas->tile_at_position(position);
		if (tily != nullptr && glm::distance(position, translate_3D_to_2D(player->position)) < 10.f) {
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
			landnav.find_2D_path(translate_3D_to_2D(player->position), translate_3D_to_2D(marker.position), waypoints);
			player->set_path(waypoints);
		} else if (player->get_movement_mode() == MOVEMENT_SEA) {
			seanav.find_2D_path(translate_3D_to_2D(player->position), translate_3D_to_2D(marker.position), waypoints);
			player->set_path(waypoints);
		}
	}
}
	
class Battle {
public:
	bool naval = false;
	CORE::Camera camera;
	PHYSICS::PhysicsManager physicsman;
	std::unique_ptr<Landscape> landscape;
	// graphics
	std::unique_ptr<RenderGroup> ordinary;
	std::unique_ptr<BillboardGroup> billboards;
	std::unique_ptr<Terrain> terrain;
	Skybox skybox;
	// entities
	Creature *player;
	std::vector<StationaryObject*> stationaries;
	std::vector<Entity> entities;
public:
	void init(const Module *mod, const CORE::Window *window, const struct shader_group_t *shaders);
	void load_assets(const Module *mod);
	void add_creatures();
	void add_buildings();
	void add_trees();
	void cleanup();
	void teardown();
};
	
void Battle::init(const Module *mod, const CORE::Window *window, const struct shader_group_t *shaders)
{
	landscape = std::make_unique<Landscape>(2048);

	load_assets(mod);

	billboards = std::make_unique<BillboardGroup>(&shaders->billboard);
	ordinary = std::make_unique<RenderGroup>(&shaders->debug);

	skybox.init(window->width, window->height);
}

void Battle::load_assets(const Module *mod)
{
	// import all the buildings of the module
	std::vector<const GLTF::Model*> house_models;
	for (const auto &house : mod->houses) {
		house_models.push_back(MediaManager::load_model(house.model));
	}
	landscape->load_buildings(house_models);
	
	terrain = std::make_unique<Terrain>(landscape->SCALE, landscape->get_heightmap(), landscape->get_normalmap(), landscape->get_sitemasks());
	terrain->add_material("STONEMAP", MediaManager::load_texture("ground/stone.dds"));
	terrain->add_material("SANDMAP", MediaManager::load_texture("ground/sand.dds"));
	terrain->add_material("GRASSMAP", MediaManager::load_texture("ground/grass.dds"));
	terrain->add_material("GRAVELMAP", MediaManager::load_texture("ground/gravel.dds"));
	terrain->add_material("DETAILMAP", MediaManager::load_texture("ground/stone_normal.dds"));
	terrain->add_material("WAVE_BUMPMAP", MediaManager::load_texture("ground/water_normal.dds"));
}
	
void Battle::add_creatures()
{
	player = new Creature { glm::vec3(3072.f, 150.f, 3072.f), glm::quat(1.f, 0.f, 0.f, 0.f) };

	physicsman.add_body(player->get_body(), PHYSICS::COLLISION_GROUP_ACTOR, PHYSICS::COLLISION_GROUP_ACTOR | PHYSICS::COLLISION_GROUP_HEIGHTMAP | PHYSICS::COLLISION_GROUP_WORLD);

	std::vector<const Entity*> ents;
	ents.push_back(player);
	ordinary->add_object(MediaManager::load_model("capsule.glb"), ents);
}
	
void Battle::add_buildings()
{
	// add houses
	const auto houses = landscape->get_houses();
	for (const auto &house : houses) {
		std::vector<const Entity*> house_entities;
		const GLTF::Model *model = house.model;
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
		physicsman.add_body(stationary->body, PHYSICS::COLLISION_GROUP_WORLD, PHYSICS::COLLISION_GROUP_ACTOR);
	}
}
	
void Battle::add_trees()
{
	const auto trees = landscape->get_trees();

	entities.clear();

	for (const auto &tree : trees) {
		Entity entity = Entity(tree.position, tree.rotation);
		entity.scale = tree.scale;
		entities.push_back(entity);
	}

	std::vector<const Entity*> ents;
	for (int i = 0; i < entities.size(); i++) {
		ents.push_back(&entities[i]);
	}

	billboards->add_billboard(MediaManager::load_texture("trees/fir.dds"), ents);
}

void Battle::cleanup()
{
	// remove stationary objects from physics
	for (const auto &stationary : stationaries) {
		physicsman.remove_body(stationary->body);
	}

	physicsman.remove_body(player->get_body());
	delete player;

	physicsman.clear();
	billboards->clear();
	ordinary->clear();

	// delete stationaries
	for (int i = 0; i < stationaries.size(); i++) {
		delete stationaries[i];
	}
	stationaries.clear();
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
	Module modular;
	enum game_state_t state;
	struct game_settings_t settings;
	Saver saver;
	CORE::Window window;
	CORE::Input input;
	CORE::Timer timer;
	TextManager *textman;
	Debugger debugger;
	Campaign campaign;
	Battle battle;
	// graphics
	RenderManager renderman;
	struct shader_group_t shaders;
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

class Engine {
public:
	int32_t run();
private:
	Game game;
};

int32_t Engine::run()
{
	if (!game.init()) {
		return EXIT_FAILURE;
	}

	game.run();

	game.shutdown();

	return EXIT_SUCCESS;
}

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

	textman = new TextManager { "fonts/exocet.ttf", 40 };

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

	shaders.billboard.compile("shaders/billboard.vert", GL_VERTEX_SHADER);
	shaders.billboard.compile("shaders/billboard.frag", GL_FRAGMENT_SHADER);
	shaders.billboard.link();

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
		state = GS_EXIT;
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
		if (ImGui::Button("Exit Battle")) { state = GS_CAMPAIGN; }
		ImGui::End();
	}

	battle.player->move(battle.camera.direction, input.key_down(SDLK_w), input.key_down(SDLK_s), input.key_down(SDLK_d), input.key_down(SDLK_a));

	input.update_keymap();

	// update creatures
	battle.player->update(battle.physicsman.get_world());

	battle.physicsman.update(timer.delta);

	battle.player->sync();

	//battle.camera.translate(battle.player->position + glm::vec3(0.f, 1.8f, 0.f));
	battle.camera.update();
	
	// update atmosphere
	battle.skybox.colorize(modular.colors.skytop, modular.colors.skybottom, glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)), settings.clouds_enabled);
	battle.skybox.update(&battle.camera, timer.elapsed);
}
	
void Game::prepare_battle()
{
	battle.camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	battle.camera.project();

	// find the campaign tile the player is on and prepare local scene properties based on it
	glm::vec2 position = translate_3D_to_2D(campaign.player->position);
	const struct tile *tily = campaign.atlas->tile_at_position(position);
	uint32_t tileref = 0;
	float amp = 0.f;
	uint8_t precipitation = 0;
	int32_t local_seed = 0;
	uint8_t site_radius = 0;
	if (tily) {
		battle.naval = !tily->land;
		amp = tily->amp;
		tileref = tily->index;
		precipitation = tily->precipitation;
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

	glm::vec3 grasscolor = glm::mix(modular.colors.grass_dry, modular.colors.grass_lush, precipitation / 255.f);

	battle.landscape->generate(campaign.seed, tileref, local_seed, amp, precipitation, site_radius, false, battle.naval);
	battle.terrain->reload(battle.landscape->get_heightmap(), battle.landscape->get_normalmap(), battle.landscape->get_sitemasks());
	battle.terrain->change_atmosphere(glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)), modular.colors.skybottom, 0.0005f);
	battle.terrain->change_grass(grasscolor);

	battle.physicsman.add_heightfield(battle.landscape->get_heightmap(), battle.landscape->SCALE, PHYSICS::COLLISION_GROUP_HEIGHTMAP, PHYSICS::COLLISION_GROUP_ACTOR);


	shaders.billboard.use();
	shaders.billboard.uniform_float("FOG_FACTOR", 0.0005f);
	shaders.billboard.uniform_vec3("FOG_COLOR", modular.colors.skybottom);

	// add entities
	battle.add_trees();
	battle.add_buildings();
	battle.add_creatures();

	battle.skybox.prepare();

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

	while (state == GS_BATTLE) {
		timer.begin();

		update_battle();

		renderman.bind_FBO();
	
		battle.ordinary->display(&battle.camera);

		if (debugmode) {
			//debugger.render_bboxes(&battle.camera);
		}

		battle.billboards->display(&battle.camera);

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
		state = GS_EXIT;
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
		if (ImGui::Button("Battle scene")) { state = GS_BATTLE; }
		if (ImGui::Button("Title screen")) { state = GS_TITLE; }
		if (ImGui::Button("Exit Game")) { state = GS_EXIT; }
		ImGui::End();
	}

	// trigger battle
	if (campaign.player->get_target_type() == TARGET_SETTLEMENT) {
		if (glm::distance(translate_3D_to_2D(campaign.player->position), translate_3D_to_2D(campaign.marker.position)) < 1.f) {
			state = GS_BATTLE;
		}
	}

	input.update_keymap();

	campaign.offset_entities();

	// update atmosphere
	campaign.skybox.colorize(modular.colors.skytop, modular.colors.skybottom, glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)), false);
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

	campaign.worldmap->reload(campaign.atlas->get_heightmap(), campaign.atlas->get_watermap(), campaign.atlas->get_rainmap(), campaign.atlas->get_materialmasks(), campaign.atlas->get_factions());
	campaign.worldmap->change_atmosphere(modular.colors.skybottom, 0.0005f, glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f)));
	campaign.worldmap->change_groundcolors(modular.colors.grass_dry, modular.colors.grass_lush);

	campaign.collisionman.add_heightfield(campaign.atlas->get_heightmap(), campaign.atlas->SCALE, PHYSICS::COLLISION_GROUP_HEIGHTMAP, PHYSICS::COLLISION_GROUP_HEIGHTMAP);
	campaign.collisionman.add_heightfield(campaign.atlas->get_watermap(), campaign.atlas->SCALE, PHYSICS::COLLISION_GROUP_HEIGHTMAP, PHYSICS::COLLISION_GROUP_HEIGHTMAP);

	campaign.camera.position = { 2048.f, 200.f, 2048.f };
	campaign.camera.lookat(glm::vec3(0.f, 0.f, 0.f));

	shaders.billboard.use();
	shaders.billboard.uniform_float("FOG_FACTOR", 0.0005f);
	shaders.billboard.uniform_vec3("FOG_COLOR", modular.colors.skybottom);

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
	state = GS_CAMPAIGN;
	
	Entity dragon = Entity(glm::vec3(2048.f, 160.f, 2048.f), glm::quat(1.f, 0.f, 0.f, 0.f));
	std::vector<const Entity*> ents;
	ents.push_back(&dragon);
	campaign.ordinary->add_object(MediaManager::load_model("dragon.glb"), ents);

	while (state == GS_CAMPAIGN) {
		timer.begin();

		update_campaign();

		renderman.bind_FBO();
	
		campaign.ordinary->display(&campaign.camera);
		campaign.creatures->display(&campaign.camera);
		campaign.billboards->display(&campaign.camera);

		campaign.worldmap->display_land(&campaign.camera);

		campaign.skybox.display(&campaign.camera);

		renderman.bind_depthmap(GL_TEXTURE2);
		campaign.worldmap->display_water(&campaign.camera, timer.elapsed);

		campaign.labelman->display(&campaign.camera);

		renderman.final_render();

		if (debugmode) {
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		window.swap();
		timer.end();

		if (state == GS_BATTLE) {
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
	
	state = GS_TITLE;
	
	textman->add_text("Hello World!", glm::vec3(1.f, 1.f, 1.f), glm::vec2(50.f, 400.f));
	textman->add_text("Archeon Prototype", glm::vec3(1.f, 1.f, 1.f), glm::vec2(50.f, 100.f));

	while (state == GS_TITLE) {
		input.update();
		if (input.exit_request()) {
			state = GS_EXIT;
		}

		if (debugmode) {
			// Start the Dear ImGui frame
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame(window.window);
			ImGui::NewFrame();
			ImGui::Begin("Main Menu Debug Mode");
			ImGui::SetWindowSize(ImVec2(400, 200));
			if (ImGui::Button("New World")) {
				state = GS_NEW_CAMPAIGN;
			}
			if (ImGui::Button("Load World")) {
				state = GS_LOAD_CAMPAIGN;
			}
			if (ImGui::Button("Exit")) { state = GS_EXIT; }
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

		if (state == GS_NEW_CAMPAIGN) {
			new_campaign();
		}
		if (state == GS_LOAD_CAMPAIGN) {
			load_campaign();
		}
	}
}

int main(int argc, char *argv[])
{
	Engine engine;
	auto status = engine.run();

	return status;
}
