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
#include "army.h"

#include "campaign.h"
#include "battle.h"
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

class Game {
public:
	bool init();
	void run();
	void shutdown();
private:
	bool running;
	bool debugmode;
	module::Module modular;
	std::string save_directory;
	enum game_state state;
	game_settings_t settings;
	util::Window window;
	util::Input input;
	util::Timer timer;
	gfx::TextManager *textman;
	Debugger debugger;
	Campaign campaign;
	Battle battle;
	// graphics
	gfx::RenderManager renderman;
	shader_group_t shaders;
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
		save_directory = savepath;
		SDL_free(savepath);
	} else {
		LOG(ERROR, "Save") << "could not find user pref path";
		return false;
	}

	textman = new gfx::TextManager { "fonts/exocet.ttf", 40 };

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

	shaders.tree.compile("shaders/battle/trees.vert", GL_VERTEX_SHADER);
	shaders.tree.compile("shaders/battle/trees.frag", GL_FRAGMENT_SHADER);
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
	battle.skybox.colorize(modular.atmosphere.day.zenith, modular.atmosphere.day.horizon, campaign.battle_info.sun_position, campaign.battle_info.ambiance_color, settings.clouds_enabled);
	battle.skybox.update(&battle.camera, timer.elapsed);
}
	
void Game::prepare_battle()
{
	battle.camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	battle.camera.project();

	// find the campaign tile the player is on and prepare local scene properties based on it
	glm::vec2 position = geom::translate_3D_to_2D(campaign.player->position);
	const auto tily = campaign.atlas.tile_at_position(position);
	uint32_t tileref = 0;
	float amp = 0.f;
	uint8_t precipitation = 0;
	uint8_t tree_density = 0;
	uint8_t temperature = 0;
	int32_t local_seed = 0;
	uint8_t site_radius = 0;
	enum geography::tile_regolith regolith = geography::tile_regolith::SAND;
	enum geography::tile_feature feature = geography::tile_feature::NONE;
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
	}

	if (campaign.player->get_target_type() == TARGET_SETTLEMENT) {
		auto search = campaign.settlements.find(campaign.battle_info.settlementID);
		if (search != campaign.settlements.end()) {
			const auto &settlement = search->second;
			site_radius = settlement.population;
		}
	}

	glm::vec3 grasscolor = glm::mix(glm::vec3(1.5f)*modular.palette.grass.min, modular.palette.grass.max, precipitation / 255.f);

	if (feature == geography::tile_feature::WOODS) {
		tree_density = 255;
	} else if (regolith == geography::tile_regolith::SNOW || regolith == geography::tile_regolith::SAND) {
		tree_density = 0;
	} else {
		tree_density = (precipitation > 64) ? 64 : precipitation;
	}

	battle.landscape->generate(campaign.seed, tileref, local_seed, amp, precipitation, temperature, tree_density, site_radius, true, battle.naval);
	battle.terrain->reload(battle.landscape->get_heightmap(), battle.landscape->get_normalmap(), battle.landscape->get_sitemasks());
	battle.terrain->change_atmosphere(campaign.battle_info.sun_position, modular.atmosphere.day.horizon, campaign.battle_info.fog_factor, campaign.battle_info.ambiance_color);
	
	glm::vec3 rock_color = glm::mix(glm::vec3(0.8f), glm::vec3(1.f), temperature / 255.f);
	
	bool grass_present = false;
	// main terrain soil
	if (regolith == geography::tile_regolith::GRASS) {
		grass_present = true;
		battle.terrain->insert_material("REGOLITH_MAP", MediaManager::load_texture("ground/grass.dds"));
	} else if (regolith == geography::tile_regolith::SAND) {
		battle.terrain->insert_material("REGOLITH_MAP", MediaManager::load_texture("ground/sand.dds"));
		grasscolor = glm::vec3(0.96, 0.83, 0.63);
		rock_color = glm::vec3(0.96, 0.83, 0.63);
	} else {
		battle.terrain->insert_material("REGOLITH_MAP", MediaManager::load_texture("ground/snow.dds"));
		grasscolor = glm::vec3(1.f);
	}

	battle.terrain->change_grass(grasscolor, grass_present);
	battle.terrain->change_rock_color(rock_color);

	// first add the heightfield
	battle.physicsman.add_heightfield(battle.landscape->get_heightmap(), battle.landscape->SCALE, physics::COLLISION_GROUP_HEIGHTMAP, physics::COLLISION_GROUP_ACTOR | physics::COLLISION_GROUP_RAY | physics::COLLISION_GROUP_RAGDOLL);

	// add entities
	battle.add_trees();
	battle.add_buildings();
	battle.add_walls();
	battle.add_creatures(&modular);

	battle.skybox.prepare();

	battle.forest->set_atmosphere(campaign.battle_info.sun_position, modular.atmosphere.day.horizon, campaign.battle_info.fog_factor, campaign.battle_info.ambiance_color);

	if (battle.naval) {
		battle.camera.position = glm::vec3(3072.f, 270.f, 3072.f);
	} else {
		battle.camera.position = glm::vec3(3072.f, 200.f, 3072.f);
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
		debugger.delete_boxes();
	}

	battle.cleanup();
}
	
void Game::update_campaign()
{
	input.update();
	if (input.exit_request()) {
		state = game_state::EXIT;
	}
	
	campaign.update_camera(&input, &window, settings.look_sensitivity, timer.delta);

	if (input.key_pressed(SDL_BUTTON_RIGHT) == true && input.mouse_grabbed() == false) {
		glm::vec3 ray = campaign.camera.ndc_to_ray(input.abs_mousecoords());
		campaign.change_player_target(ray);
	}

	campaign.player->update(timer.delta);
	campaign.player_army.position = { campaign.player->position.x, campaign.player->position.z };

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
		if (ImGui::Button("Save game")) { campaign.save(save_directory + "game.save"); }
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
	campaign.skybox.colorize(modular.atmosphere.day.zenith, modular.atmosphere.day.horizon, campaign.battle_info.sun_position, campaign.battle_info.ambiance_color, false);
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

	campaign.atlas.generate(campaign.seed, &modular.params);

	campaign.atlas.create_land_navigation();
	const auto land_navsoup = campaign.atlas.get_navsoup();
	campaign.landnav.build(land_navsoup.vertices, land_navsoup.indices);

	campaign.atlas.create_sea_navigation();
	const auto sea_navsoup = campaign.atlas.get_navsoup();
	campaign.seanav.build(sea_navsoup.vertices, sea_navsoup.indices);
	
	campaign.spawn_settlements();

	campaign.player_army.position = glm::vec2(2010.f, 2010.f);
	campaign.player_army.name = "Player's Army";
	campaign.camera.position = glm::vec3(2048.f, 200.f, 2048.f);
	campaign.camera.lookat(glm::vec3(0.f, 0.f, 0.f));

	prepare_campaign();

	run_campaign();
}
	
void Game::load_campaign()
{
	campaign.load(save_directory + "game.save");

	//campaign.add_heightfields();

	prepare_campaign();

	run_campaign();
}

void Game::prepare_campaign()
{
	campaign.battle_info.sun_position = glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f));

	campaign.camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	campaign.camera.project();

	// campaign world map data
	campaign.atlas.create_mapdata(campaign.seed);

	const auto terragen = campaign.atlas.get_terragen();
	campaign.worldmap->reload(&terragen->heightmap, campaign.atlas.get_watermap(), &terragen->rainmap, campaign.atlas.get_materialmasks(), campaign.atlas.get_factions());
	campaign.worldmap->reload_temperature(&terragen->tempmap);
	campaign.worldmap->change_atmosphere(modular.atmosphere.day.horizon, 0.0002f, campaign.battle_info.sun_position);
	campaign.worldmap->change_groundcolors(modular.palette.grass.min, modular.palette.grass.max, modular.palette.rock_base.min, modular.palette.rock_base.max, modular.palette.rock_desert.min, modular.palette.rock_desert.max);

	campaign.camera.direct(campaign.camera.direction);

	campaign.add_heightfields();

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
		shaders.object.uniform_vec3("SUN_POS", glm::normalize(campaign.battle_info.sun_position));
		if (!campaign.factions_visible) {
			campaign.creatures->display(&campaign.camera);
		}

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
