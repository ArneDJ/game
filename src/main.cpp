#include <iostream>
#include <unordered_map>
#include <chrono>
#include <map>
#include <random>
#include <vector>
#include <list>
#include <span>

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
#include "core/sky.h"
#include "core/texture.h"
#include "core/model.h"
#include "core/render.h"
#include "core/physics.h"
#include "core/animation.h"
#include "core/navigation.h"
#include "core/voronoi.h"
#include "object.h"
#include "debugger.h"
#include "module.h"
#include "terra.h"
#include "worldgraph.h"
#include "atlas.h"
#include "worldmap.h"
#include "save.h"
//#include "core/sound.h" // TODO replace SDL_Mixer with OpenAL

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
	std::string savedir;
	bool saveable;
	Saver saver;
	WindowManager windowman;
	InputManager inputman;
	PhysicsManager physicsman;
	Timer timer;
	Camera camera;
	Navigation navigation;
	Debugger debugger;
	// random seed generator
	std::random_device rd;
	std::uniform_int_distribution<long> dis;
	// campaign data
	Atlas *atlas;
	long seed;
	// graphics
	RenderManager renderman;
	Skybox skybox;
	Worldmap *worldmap;
	Shader object_shader;
	Shader debug_shader;
	// temporary assets
	GLTF::Model *duck;
	GLTF::Model *dragon;
	Texture *relief;
	Texture *biomes;
private:
	void init(void);
	void init_settings(void);
	void load_assets(void);
	void new_campaign(void);
	void load_campaign(void);
	void run_campaign(void);
	void update_campaign(void);
	void teardown(void);
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
}

void Game::init(void)
{
	// load settings
	init_settings();

	// load module data
	modular.load("native");

	if (!windowman.init(settings.window_width, settings.window_height)) {
		exit(EXIT_FAILURE);
	}

	if (settings.fullscreen) {
		windowman.set_fullscreen();
	}
	
	SDL_SetRelativeMouseMode(SDL_FALSE);

	renderman.init();

	// load shaders
	debug_shader.compile("shaders/debug.vert", GL_VERTEX_SHADER);
	debug_shader.compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	debug_shader.link();

	object_shader.compile("shaders/object.vert", GL_VERTEX_SHADER);
	object_shader.compile("shaders/object.frag", GL_FRAGMENT_SHADER);
	object_shader.link();

	camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	camera.project();

	skybox.init(modular.atmos.skytop, modular.atmos.skybottom);

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
		saveable = true;
		savedir = savepath;
		SDL_free(savepath);
	} else {
		saveable = false;
		write_log(LogType::ERROR, "Save error: could not find user pref path");
	}

	atlas = new Atlas { 2048, 512, 512 };

	worldmap = new Worldmap { atlas->scale, atlas->get_heightmap(), atlas->get_rainmap() };

	relief = new Texture { atlas->get_relief() };
	relief->change_wrapping(GL_CLAMP_TO_EDGE);
	biomes = new Texture { atlas->get_biomes() };
	biomes->change_wrapping(GL_CLAMP_TO_EDGE);
}

void Game::teardown(void)
{
	delete dragon;
	delete duck;

	delete worldmap;

	delete atlas;

	delete relief;
	delete biomes;

	if (debugmode) {
		debugger.teardown();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
	}

	skybox.teardown();
	//renderman.teardown();

	windowman.teardown();
}
	
void Game::load_assets(void)
{
	duck = new GLTF::Model { "modules/native/media/models/duck.glb", "modules/native/media/textures/duck.dds" };
	dragon = new GLTF::Model { "modules/native/media/models/dragon.glb", "" };
}

void Game::update_campaign(void)
{
	inputman.update();
	if (inputman.exit_request()) {
		state = GS_EXIT;
	}
	
	glm::vec2 rel_mousecoords = settings.look_sensitivity * inputman.rel_mousecoords();
	camera.target(rel_mousecoords);

	float modifier = 20.f * timer.delta;
	if (inputman.key_down(SDLK_w)) { camera.move_forward(modifier); }
	if (inputman.key_down(SDLK_s)) { camera.move_backward(modifier); }
	if (inputman.key_down(SDLK_d)) { camera.move_right(modifier); }
	if (inputman.key_down(SDLK_a)) { camera.move_left(modifier); }

	camera.update();

	physicsman.update(timer.delta);

	if (debugmode) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(windowman.window);
		ImGui::NewFrame();
		ImGui::Begin("Campaign Debug Mode");
		ImGui::SetWindowSize(ImVec2(400, 200));
		ImGui::Text("ms per frame: %d", timer.ms_per_frame);
		ImGui::Text("cam position: %f, %f, %f", camera.position.x, camera.position.y, camera.position.z);
		if (ImGui::Button("Title screen")) { state = GS_TITLE; }
		if (ImGui::Button("Exit Game")) { state = GS_EXIT; }
		ImGui::End();
	}

	inputman.update_keymap();
}
	
void Game::new_campaign(void)
{
	// generate a new seed
	std::mt19937 gen(rd());
	seed = dis(gen);
	seed = 1337;

	write_log(LogType::RUN, "seed: " + std::to_string(seed));

	atlas->generate(seed, &modular.params);

	atlas->create_maps();

	if (saveable) {
		saver.save(savedir + "game.save", atlas);
	}

	run_campaign();
}
	
void Game::load_campaign(void)
{
	auto start = std::chrono::steady_clock::now();
	saver.load(savedir + "game.save", atlas);
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";

	atlas->create_maps();

	run_campaign();
}

void Game::run_campaign(void)
{
	state = GS_CAMPAIGN;

	camera.position = { 2048.f, 200.f, 2048.f };
	camera.lookat(glm::vec3(0.f, 0.f, 0.f));

	worldmap->reload(atlas->get_heightmap(), atlas->get_rainmap());

	relief->reload(atlas->get_relief());
	biomes->reload(atlas->get_biomes());

	while (state == GS_CAMPAIGN) {
		timer.begin();

		update_campaign();

		renderman.prepare_to_render();

		debug_shader.use();
		debug_shader.uniform_mat4("VP", camera.VP);
		debug_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), glm::vec3(2048.f, 160.f, 2048.f)));
		dragon->display();

		object_shader.use();
		object_shader.uniform_mat4("VP", camera.VP);
		object_shader.uniform_bool("INSTANCED", false);

		object_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), glm::vec3(2010.f, 200.f, 2010.f)));
		duck->display();

		relief->bind(GL_TEXTURE2);
		biomes->bind(GL_TEXTURE3);
		worldmap->display(&camera);

		skybox.display(&camera);

		if (debugmode) {
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		windowman.swap();
		timer.end();
	}

	physicsman.clear();
}

void Game::run(void)
{
	state = GS_TITLE;

	init();
	load_assets();

	while (state == GS_TITLE) {
		inputman.update();
		if (inputman.exit_request()) {
			state = GS_EXIT;
		}

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
		
		renderman.prepare_to_render();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
