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

#include "extern/inih/INIReader.h"

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "core/logger.h"
#include "core/geom.h"
#include "core/image.h"
#include "core/entity.h"
#include "core/camera.h"
#include "core/window.h"
#include "core/input.h"
#include "core/timer.h"
#include "core/shader.h"
#include "core/mesh.h"
#include "core/texture.h"
#include "core/model.h"
#include "core/physics.h"
#include "core/animation.h"
#include "core/navigation.h"
#include "core/voronoi.h"
#include "core/media.h"
#include "graphics/clouds.h"
#include "graphics/sky.h"
#include "graphics/render.h"
#include "graphics/shadow.h"
#include "graphics/terrain.h"
#include "graphics/worldmap.h"
#include "object.h"
#include "debugger.h"
#include "module.h"
#include "terragen.h"
#include "worldgraph.h"
#include "mapfield.h"
#include "atlas.h"
#include "save.h"
#include "army.h"
#include "landscape.h"
//#include "core/sound.h" // TODO replace SDL_Mixer with OpenAL

//static const glm::vec3 sun_position = glm::normalize(glm::vec3(0.5f, 0.5f, 0.5f));
static const glm::vec3 sun_position = glm::normalize(glm::vec3(0.5f, 0.93f, 0.1f));
static const glm::vec3 grass_dry = glm::vec3(1.f, 1.f, 0.2f);
static const glm::vec3 grass_verdant = glm::vec3(0.7f, 1.f, 0.2f);

enum game_state {
	GS_TITLE,
	GS_NEW_CAMPAIGN,
	GS_LOAD_CAMPAIGN,
	GS_CAMPAIGN,
	GS_BATTLE,
	GS_EXIT
};

struct game_settings {
	uint16_t window_width;
	uint16_t window_height;
	bool fullscreen;
	int FOV;
	float look_sensitivity;
	// graphics
	bool clouds_enabled;
};

struct Campaign {
	long seed;
	Navigation landnav;
	Navigation seanav;
	Camera camera;
	btRigidBody *surface;
	btRigidBody *watersurface;
	Atlas *atlas;
	Entity marker;
	Worldmap *worldmap;
	Army *player;
	RenderGroup *ordinary;
	RenderGroup *creatures;
};

struct Battle {
	Camera camera;
	RenderGroup *ordinary;
	RenderGroup *shadowcasters;
	Terrain *terrain;
	btRigidBody *surface;
	Landscape *landscape;
};

class Game {
public:
	void run(void);
private:
	bool running;
	bool debugmode;
	Module modular;
	enum game_state state;
	struct game_settings settings;
	Saver saver;
	WindowManager windowman;
	InputManager inputman;
	PhysicsManager physicsman;
	Timer timer;
	Debugger debugger;
	struct Campaign campaign;
	struct Battle battle;
	// graphics
	MediaManager mediaman;
	RenderManager renderman;
	FrameSystem *framesystem;
	Skybox skybox;
	Shader object_shader;
	Shader debug_shader;
	Shader depth_shader;
	Shader copy_shader;
	Shadow *shadow;
	// temporary assets
	bool show_cascades = false;
private:
	void init(void);
	void init_settings(void);
	void load_module(void);
	void reserve_battle(void);
	void run_battle(void);
	void update_battle(void);
	void reserve_campaign(void);
	void new_campaign(void);
	void load_campaign(void);
	void run_campaign(void);
	void update_campaign(void);
	void cleanup_campaign(void);
	void teardown(void);
	void teardown_campaign(void);
	void teardown_battle(void);
};

void Game::init_settings(void)
{
	static const std::string INI_SETTINGS_PATH = "settings.ini";
	INIReader reader = { INI_SETTINGS_PATH.c_str() };
	if (reader.ParseError() != 0) { 
		write_log(LogType::ERROR, std::string("Could not load ini file: " + INI_SETTINGS_PATH));
	}
	settings.window_width = reader.GetInteger("", "WINDOW_WIDTH", 1920);
	settings.window_height = reader.GetInteger("", "WINDOW_HEIGHT", 1080);
	settings.fullscreen = reader.GetBoolean("", "FULLSCREEN", false);
	settings.FOV = reader.GetInteger("", "FOV", 90);
	settings.look_sensitivity = float(reader.GetInteger("", "MOUSE_SENSITIVITY", 1));
	debugmode = reader.GetBoolean("", "DEBUG_MODE", false);
	
	// graphics settings
	settings.clouds_enabled = reader.GetBoolean("", "CLOUDS_ENABLED", false);
}

void Game::init(void)
{
	// load settings
	init_settings();

	if (!windowman.init(settings.window_width, settings.window_height)) {
		exit(EXIT_FAILURE);
	}

	if (settings.fullscreen) {
		windowman.set_fullscreen();
	}
	
	SDL_SetRelativeMouseMode(SDL_FALSE);

	renderman.init();
	framesystem = new FrameSystem { settings.window_width, settings.window_height };
	shadow = new Shadow { 4096 };

	if (debugmode) {
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO(); (void)io;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer bindings
		ImGui_ImplSDL2_InitForOpenGL(windowman.window, windowman.glcontext);
		ImGui_ImplOpenGL3_Init("#version 430");
	}

	// find the save directory on the system
	char *savepath = SDL_GetPrefPath("archeon", "saves");
	if (savepath) {
		saver.change_directory(savepath);
		SDL_free(savepath);
	} else {
		write_log(LogType::ERROR, "Save error: could not find user pref path");
	}
}
	
void Game::load_module(void)
{
	std::string modulename = "native";

	modular.load(modulename);

	mediaman.change_path(modular.path);

	// load assets

	// load shaders
	debug_shader.compile("shaders/debug.vert", GL_VERTEX_SHADER);
	debug_shader.compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	debug_shader.link();

	object_shader.compile("shaders/object.vert", GL_VERTEX_SHADER);
	object_shader.compile("shaders/object.frag", GL_FRAGMENT_SHADER);
	object_shader.link();

	depth_shader.compile("shaders/depthmap.vert", GL_VERTEX_SHADER);
	depth_shader.compile("shaders/depthmap.frag", GL_FRAGMENT_SHADER);
	depth_shader.link();

	copy_shader.compile("shaders/copy.comp", GL_COMPUTE_SHADER);
	copy_shader.link();

	skybox.init(windowman.width, windowman.height);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	reserve_campaign();

	reserve_battle();

	physicsman.add_ground_plane(glm::vec3(0.f, -1.f, 0.f));
}

void Game::teardown(void)
{
	delete framesystem;

	delete shadow;

	teardown_campaign();

	teardown_battle();

	mediaman.teardown();

	if (debugmode) {
		debugger.teardown();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
	}

	skybox.teardown();
	//renderman.teardown();

	physicsman.clear();

	windowman.teardown();
}

void Game::teardown_campaign(void)
{
	delete campaign.ordinary;
	delete campaign.creatures;

	delete campaign.player;

	delete campaign.worldmap;

	delete campaign.atlas;

	delete campaign.surface;
	delete campaign.watersurface;
}

void Game::teardown_battle(void)
{
	delete battle.ordinary;
	delete battle.shadowcasters;

	delete battle.landscape;

	delete battle.terrain;

	delete battle.surface;
}
	
void Game::update_battle(void)
{
	inputman.update();
	if (inputman.exit_request()) {
		state = GS_EXIT;
	}

	glm::vec2 rel_mousecoords = settings.look_sensitivity * inputman.rel_mousecoords();
	battle.camera.target(rel_mousecoords);

	float modifier = 20.f * timer.delta;
	if (inputman.key_down(SDLK_w)) { battle.camera.move_forward(modifier); }
	if (inputman.key_down(SDLK_s)) { battle.camera.move_backward(modifier); }
	if (inputman.key_down(SDLK_d)) { battle.camera.move_right(modifier); }
	if (inputman.key_down(SDLK_a)) { battle.camera.move_left(modifier); }

	battle.camera.update();

	if (debugmode) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(windowman.window);
		ImGui::NewFrame();
		ImGui::Begin("Battle Debug Mode");
		ImGui::SetWindowSize(ImVec2(400, 200));
		ImGui::Text("seed: %d", campaign.seed);
		ImGui::Text("ms per frame: %d", timer.ms_per_frame);
		ImGui::Text("cam position: %f, %f, %f", battle.camera.position.x, battle.camera.position.y, battle.camera.position.z);
		ImGui::Checkbox("Show Cascades", &show_cascades);
		if (ImGui::Button("Exit Battle")) { state = GS_CAMPAIGN; }
		ImGui::End();
	}

	inputman.update_keymap();
	
	physicsman.update(timer.delta);
	
	// update atmosphere
	skybox.change_atmosphere(modular.atmos.skytop, modular.atmos.skybottom, sun_position, settings.clouds_enabled);
	skybox.update(&battle.camera, timer.elapsed);
}

void Game::run_battle(void)
{
	battle.camera.position = { 3072.f, 200.f, 3072.f };
	battle.camera.lookat(glm::vec3(0.f, 0.f, 0.f));

	glm::vec2 position = translate_3D_to_2D(campaign.player->position);
	const struct tile *tily = campaign.atlas->tile_at_position(position);
	uint32_t offset = 0;
	float amp = 0.f;
	uint8_t precipitation = 0;
	if (tily) {
		amp = tily->amp;
		offset = tily->index;
		precipitation = tily->precipitation;
	}
	glm::vec3 grasscolor = glm::mix(grass_dry, grass_verdant, precipitation / 255.f);

	battle.landscape->generate(campaign.seed, offset, amp);
	battle.terrain->reload(battle.landscape->get_heightmap(), battle.landscape->get_normalmap());
	battle.terrain->change_atmosphere(sun_position, modular.atmos.skybottom, 0.0005f);
	battle.terrain->change_grass(grasscolor);

	physicsman.insert_body(battle.surface);

	// testing entities
	btCollisionShape *sphere_shape = physicsman.add_sphere(1.f);
	btCollisionShape *cube_shape = physicsman.add_box(glm::vec3(1.f, 1.f, 1.f));
	btCollisionShape *cylinder_shape = physicsman.add_cylinder(glm::vec3(1.f, 1.f, 1.f));
	btCollisionShape *capsule_shape = physicsman.add_capsule(0.5f, 1.f);
	DynamicObject cubeobject = { glm::vec3(3072.f, 215.f, 3072.f), glm::quat(1.f, 0.f, 0.f, 0.f), cube_shape };
	DynamicObject cylinderobject = { glm::vec3(3072.f, 220.f, 3072.f), glm::quat(1.f, 0.f, 0.f, 0.f), cylinder_shape };
	DynamicObject capsuleobject = { glm::vec3(3072.f, 225.f, 3072.f), glm::quat(1.f, 0.f, 0.f, 0.f), capsule_shape };
	physicsman.insert_body(cubeobject.body);
	physicsman.insert_body(cylinderobject.body);
	physicsman.insert_body(capsuleobject.body);

	std::vector<const Entity*> ents;

	DynamicObject sphereobject = { glm::vec3(3072.f, 210.f, 3072.f), glm::quat(1.f, 0.f, 0.f, 0.f), sphere_shape };
	physicsman.insert_body(sphereobject.body);
	ents.push_back(&sphereobject);
	battle.ordinary->add_object(mediaman.load_model("sphere.glb"), ents);
	battle.shadowcasters->add_object(mediaman.load_model("sphere.glb"), ents);

	ents.clear();
	ents.push_back(&cubeobject);
	battle.ordinary->add_object(mediaman.load_model("cube.glb"), ents);
	battle.shadowcasters->add_object(mediaman.load_model("cube.glb"), ents);
	ents.clear();
	ents.push_back(&cylinderobject);
	battle.ordinary->add_object(mediaman.load_model("cylinder.glb"), ents);
	battle.shadowcasters->add_object(mediaman.load_model("cylinder.glb"), ents);
	ents.clear();
	ents.push_back(&capsuleobject);
	battle.ordinary->add_object(mediaman.load_model("capsule.glb"), ents);
	battle.shadowcasters->add_object(mediaman.load_model("capsule.glb"), ents);
	
	skybox.pre_step();

	while (state == GS_BATTLE) {
		timer.begin();

		update_battle();
		sphereobject.update();
		cubeobject.update();
		cylinderobject.update();
		capsuleobject.update();

		// update cascaded shadows
		shadow->update(&battle.camera, sun_position);
		shadow->enable();
		depth_shader.use();
		const auto shadowspaces = shadow->get_shadowspaces();
		for (int i = 0; i < shadowspaces.size(); i++) {
			// render into shadow depthmap
			shadow->bind_depthmap(i);
			depth_shader.uniform_mat4("DEPTH_VP", shadowspaces[i]);
			battle.shadowcasters->display(&battle.camera);
		}
		shadow->disable();

		glViewport(0, 0, settings.window_width, settings.window_height);

		framesystem->bind();
	
		battle.ordinary->display(&battle.camera);

		battle.terrain->update_shadow(shadow, show_cascades);
		battle.terrain->display_land(&battle.camera);

		skybox.display(&battle.camera);

		// copy the current depth buffer
		framesystem->copy_depthmap();

		framesystem->unbind();

		renderman.prepare_to_render();

		framesystem->display();

		if (debugmode) {
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		windowman.swap();
		timer.end();
	}
	
	battle.ordinary->clear();
	battle.shadowcasters->clear();
	physicsman.remove_body(sphereobject.body);
	physicsman.remove_body(cubeobject.body);
	physicsman.remove_body(battle.surface);
	physicsman.remove_body(cylinderobject.body);
	physicsman.remove_body(capsuleobject.body);
	// TODO shapes need to be deleted
}

void Game::reserve_battle(void)
{
	battle.camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	battle.camera.project();

	battle.landscape = new Landscape { 2048 };
	
	battle.terrain = new Terrain { battle.landscape->SCALE, battle.landscape->get_heightmap(), battle.landscape->get_normalmap() };
	std::vector<const Texture*> materials;
	materials.push_back(mediaman.load_texture("ground/stone.dds"));
	materials.push_back(mediaman.load_texture("ground/sand.dds"));
	materials.push_back(mediaman.load_texture("ground/grass.dds"));
	materials.push_back(mediaman.load_texture("ground/stone_normal.dds"));
	materials.push_back(mediaman.load_texture("ground/water_normal.dds"));
	battle.terrain->load_materials(materials);

	battle.surface = physicsman.add_heightfield(battle.landscape->get_heightmap(), battle.landscape->SCALE);
}

void Game::reserve_campaign(void)
{
	campaign.camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	campaign.camera.project();

	campaign.atlas = new Atlas;

	campaign.worldmap = new Worldmap { campaign.atlas->SCALE, campaign.atlas->get_heightmap(), campaign.atlas->get_watermap(), campaign.atlas->get_rainmap(), campaign.atlas->get_materialmasks() };
	std::vector<const Texture*> materials;
	materials.push_back(mediaman.load_texture("ground/stone.dds"));
	materials.push_back(mediaman.load_texture("ground/sand.dds"));
	materials.push_back(mediaman.load_texture("ground/snow.dds"));
	materials.push_back(mediaman.load_texture("ground/grass.dds"));
	materials.push_back(mediaman.load_texture("ground/water_normal.dds"));
	campaign.worldmap->load_materials(materials);

	campaign.surface = physicsman.add_heightfield(campaign.atlas->get_heightmap(), campaign.atlas->SCALE);
	campaign.watersurface = physicsman.add_heightfield(campaign.atlas->get_watermap(), campaign.atlas->SCALE);

	campaign.ordinary = new RenderGroup { &debug_shader };
	campaign.creatures = new RenderGroup { &object_shader };
	battle.ordinary = new RenderGroup { &debug_shader };
	battle.shadowcasters = new RenderGroup { &depth_shader };

	glm::vec2 startpos = { 2010.f, 2010.f };
	campaign.player = new Army { startpos, 20.f };
}

void Game::cleanup_campaign(void)
{
	campaign.ordinary->clear();
	campaign.creatures->clear();

	if (debugmode) {
		debugger.delete_navmeshes();
	}
	physicsman.remove_body(campaign.surface);
	physicsman.remove_body(campaign.watersurface);
	campaign.landnav.cleanup();
	campaign.seanav.cleanup();
}

void Game::update_campaign(void)
{
	inputman.update();
	if (inputman.exit_request()) {
		state = GS_EXIT;
	}
	
	glm::vec2 rel_mousecoords = settings.look_sensitivity * inputman.rel_mousecoords();
	campaign.camera.target(rel_mousecoords);

	float modifier = 20.f * timer.delta;
	if (inputman.key_down(SDLK_w)) { campaign.camera.move_forward(modifier); }
	if (inputman.key_down(SDLK_s)) { campaign.camera.move_backward(modifier); }
	if (inputman.key_down(SDLK_d)) { campaign.camera.move_right(modifier); }
	if (inputman.key_down(SDLK_a)) { campaign.camera.move_left(modifier); }

	campaign.camera.update();
		
	struct ray_result camresult = physicsman.cast_ray(glm::vec3(campaign.camera.position.x, campaign.atlas->SCALE.y, campaign.camera.position.z), glm::vec3(campaign.camera.position.x, 0.f, campaign.camera.position.z));
	if (camresult.hit) {
		float yoffset = camresult.point.y + 10;
		if (campaign.camera.position.y < yoffset) {
			campaign.camera.position.y = yoffset;
		}
	}

	if (inputman.key_pressed(SDL_BUTTON_RIGHT) == true && inputman.mouse_grabbed() == false) {
		glm::vec3 ray = campaign.camera.ndc_to_ray(inputman.abs_mousecoords());
		struct ray_result result = physicsman.cast_ray(campaign.camera.position, campaign.camera.position + (1000.f * ray));
		if (result.hit) {
			campaign.marker.position = result.point;
			std::list<glm::vec2> waypoints;
			campaign.landnav.find_2D_path(translate_3D_to_2D(campaign.player->position), translate_3D_to_2D(campaign.marker.position), waypoints);
			campaign.player->set_path(waypoints);
		}
	}

	campaign.player->update(timer.delta);

	if (debugmode) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(windowman.window);
		ImGui::NewFrame();
		ImGui::Begin("Campaign Debug Mode");
		ImGui::SetWindowSize(ImVec2(400, 200));
		ImGui::Text("seed: %d", campaign.seed);
		ImGui::Text("ms per frame: %d", timer.ms_per_frame);
		ImGui::Text("cam position: %f, %f, %f", campaign.camera.position.x, campaign.camera.position.y, campaign.camera.position.z);
		ImGui::Text("player position: %f, %f, %f", campaign.player->position.x, campaign.player->position.y, campaign.player->position.z);
		if (ImGui::Button("Battle scene")) { state = GS_BATTLE; }
		if (ImGui::Button("Title screen")) { state = GS_TITLE; }
		if (ImGui::Button("Exit Game")) { state = GS_EXIT; }
		ImGui::End();
	}

	inputman.update_keymap();

	glm::vec3 origin = { campaign.player->position.x, campaign.atlas->SCALE.y, campaign.player->position.z };
	glm::vec3 end = { campaign.player->position.x, 0.f, campaign.player->position.z };
	struct ray_result result = physicsman.cast_ray(origin, end);
	campaign.player->set_y_offset(result.point.y);

	// update atmosphere
	skybox.change_atmosphere(modular.atmos.skytop, modular.atmos.skybottom, sun_position, false);
	skybox.update(&campaign.camera, timer.elapsed);
}
	
void Game::new_campaign(void)
{
	// generate a new seed
	std::random_device rd;
	std::uniform_int_distribution<long> dis;
	std::mt19937 gen(rd());
	campaign.seed = dis(gen);
	//campaign.seed = 1337;
	//campaign.seed = 4998651408012010310;

	write_log(LogType::RUN, "seed: " + std::to_string(campaign.seed));

	campaign.atlas->generate(campaign.seed, &modular.params);

	campaign.atlas->create_mapdata();

	campaign.atlas->create_land_navigation();
	const auto land_navsoup = campaign.atlas->get_navsoup();
	campaign.landnav.build(land_navsoup->vertices, land_navsoup->indices);

	campaign.atlas->create_sea_navigation();
	const auto sea_navsoup = campaign.atlas->get_navsoup();
	campaign.seanav.build(sea_navsoup->vertices, sea_navsoup->indices);

	saver.save("game.save", campaign.atlas, &campaign.landnav, &campaign.seanav, campaign.seed);
	
	run_campaign();
}
	
void Game::load_campaign(void)
{
	auto start = std::chrono::steady_clock::now();
	saver.load("game.save", campaign.atlas, &campaign.landnav, &campaign.seanav, campaign.seed);
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";

	campaign.atlas->create_mapdata();

	run_campaign();
}

void Game::run_campaign(void)
{
	state = GS_CAMPAIGN;

	std::vector<const Entity*> ents;
	ents.push_back(campaign.player);
	campaign.creatures->add_object(mediaman.load_model("duck.glb"), ents);

	Entity dragon = { glm::vec3(2048.f, 160.f, 2048.f), glm::quat(1.f, 0.f, 0.f, 0.f) };
	ents.clear();
	ents.push_back(&campaign.marker);
	campaign.ordinary->add_object(mediaman.load_model("cone.glb"), ents);
	ents.clear();
	ents.push_back(&dragon);
	campaign.ordinary->add_object(mediaman.load_model("dragon.glb"), ents);

	campaign.camera.position = { 2048.f, 200.f, 2048.f };
	campaign.camera.lookat(glm::vec3(0.f, 0.f, 0.f));

	campaign.marker.position = { 2010.f, 200.f, 2010.f };

	campaign.worldmap->reload(campaign.atlas->get_heightmap(), campaign.atlas->get_watermap(), campaign.atlas->get_rainmap(), campaign.atlas->get_materialmasks());
	campaign.worldmap->change_atmosphere(modular.atmos.skybottom, 0.0005f, sun_position);
	campaign.worldmap->change_grass(grass_dry, grass_verdant);

	physicsman.insert_body(campaign.surface);
	physicsman.insert_body(campaign.watersurface);

	campaign.player->teleport(glm::vec2(2010.f, 2010.f));
	
	while (state == GS_CAMPAIGN) {
		timer.begin();

		update_campaign();

		framesystem->bind();
	
		campaign.ordinary->display(&campaign.camera);

		campaign.creatures->display(&campaign.camera);

		campaign.worldmap->display_land(&campaign.camera);

		skybox.display(&campaign.camera);

		framesystem->copy_depthmap();
		
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, framesystem->get_depthmap_copy());
		campaign.worldmap->display_water(&campaign.camera, timer.elapsed);

		framesystem->unbind();

		renderman.prepare_to_render();
		
		framesystem->display();

		if (debugmode) {
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		windowman.swap();
		timer.end();

		if (state == GS_BATTLE) {
			physicsman.remove_body(campaign.surface);
			physicsman.remove_body(campaign.watersurface);
			run_battle();
			physicsman.insert_body(campaign.surface);
			physicsman.insert_body(campaign.watersurface);
		}
	}

	cleanup_campaign();
}

void Game::run(void)
{
	state = GS_TITLE;

	init();
	load_module();
	
	while (state == GS_TITLE) {
		inputman.update();
		if (inputman.exit_request()) {
			state = GS_EXIT;
		}

		if (debugmode) {
			// Start the Dear ImGui frame
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame(windowman.window);
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
		
		renderman.prepare_to_render();

		if (debugmode) {
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		windowman.swap();

		if (state == GS_NEW_CAMPAIGN) {
			new_campaign();
		}
		if (state == GS_LOAD_CAMPAIGN) {
			load_campaign();
		}
	}

	teardown();
}

int main(int argc, char *argv[])
{
	write_start_log();

	Game archeon;
	archeon.run();

	write_exit_log();

	return 0;
}
