#include <iostream>
#include <unordered_map>
#include <chrono>
#include <map>
#include <vector>
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
#include <bullet/btBulletDynamicsCommon.h>

#include "extern/inih/INIReader.h"

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "core/logger.h"
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
#include "object.h"
#include "debugger.h"
#include "module.h"
#include "terra.h"
//#include "core/sound.h" // TODO replace SDL_Mixer with OpenAL

enum game_state {
	GAME_STATE_TITLE,
	GAME_STATE_CAMPAIGN,
	GAME_STATE_BATTLE,
	GAME_STATE_EXIT
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
	Module mod;
	enum game_state state;
	struct game_settings settings;
	std::string savedir;
	bool saveable;
	WindowManager windowman;
	InputManager inputman;
	PhysicsManager physicsman;
	Timer timer;
	Camera camera;
	Navigation navigation;
	Debugger debugger;
	Terragen *terragen;
	// graphics
	RenderManager renderman;
	Skybox skybox;
	Shader object_shader;
	Shader debug_shader;
	// temporary assets
	GLTF::Model *duck;
	GLTF::Model *dragon;
private:
	void init(void);
	void init_settings(void);
	void load_assets(void);
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
	mod.init("native");

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

	skybox.init(glm::vec3(0.447f, 0.639f, 0.784f), glm::vec3(0.647f, 0.623f, 0.672f));

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
	char *prefpath = SDL_GetPrefPath("archeon", "saves");
	if (prefpath) {
		saveable = true;
		savedir = prefpath;
		SDL_free(prefpath);
	} else {
		saveable = false;
		write_log(LogType::ERROR, "Save error: could not find user pref path");
	}

	terragen = new Terragen { 2048, 512, 512 };
}

void Game::teardown(void)
{
	delete dragon;
	delete duck;

	delete terragen;

	if (debugmode) {
		debugger.teardown();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
	}

	skybox.teardown();

	windowman.teardown();
}
	
void Game::load_assets(void)
{
	if (debugmode) {
		debugger.add_grid(glm::vec2(-20.f, -20.f), glm::vec2(20.f, 20.f));
	}

	duck = new GLTF::Model { "media/models/duck.glb", "media/textures/duck.dds" };
	dragon = new GLTF::Model { "media/models/dragon.glb", "" };
}

void Game::update_campaign(void)
{
	inputman.update();
	if (inputman.exit_request()) {
		state = GAME_STATE_EXIT;
	}
	
	glm::vec2 rel_mousecoords = settings.look_sensitivity * inputman.rel_mousecoords();
	camera.target(rel_mousecoords);

	float modifier = 2.f * timer.delta;
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
		if (ImGui::Button("Title screen")) { state = GAME_STATE_TITLE; }
		if (ImGui::Button("Exit Game")) { state = GAME_STATE_EXIT; }
		ImGui::End();
	}

	inputman.update_keymap();
}

void Game::run_campaign(void)
{
	state = GAME_STATE_CAMPAIGN;

	camera.position = { 10.f, 5.f, -10.f };
	camera.lookat(glm::vec3(0.f, 0.f, 0.f));

	auto start = std::chrono::steady_clock::now();
	terragen->generate(1337, &mod.params);
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
	if (saveable) {
		terragen->rainmap->write(savedir + "rain.png");
		terragen->tempmap->write(savedir + "temperature.png");
	}

	while (state == GAME_STATE_CAMPAIGN) {
		timer.begin();

		update_campaign();

		renderman.prepare_to_render();

		debug_shader.use();
		debug_shader.uniform_mat4("VP", camera.VP);
		debug_shader.uniform_mat4("MODEL", glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f)));
		dragon->display();

		if (debugmode) {
			debug_shader.uniform_mat4("MODEL", glm::mat4(1.f));
			debugger.render_grids();
			debugger.render_navmeshes();
		}
	
		object_shader.use();
		object_shader.uniform_mat4("VP", camera.VP);
		object_shader.uniform_bool("INSTANCED", false);

		object_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), glm::vec3(10.f, 0.f, 10.f)));
		duck->display();

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
	state = GAME_STATE_TITLE;

	init();
	load_assets();

	while (state == GAME_STATE_TITLE) {
		inputman.update();
		if (inputman.exit_request()) {
			state = GAME_STATE_EXIT;
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(windowman.window);
		ImGui::NewFrame();
		ImGui::Begin("Main Menu Debug Mode");
		ImGui::SetWindowSize(ImVec2(400, 200));
		if (ImGui::Button("New World")) {
			state = GAME_STATE_CAMPAIGN;
		}
		if (ImGui::Button("Exit")) { state = GAME_STATE_EXIT; }
		ImGui::End();
		
		renderman.prepare_to_render();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		windowman.swap();

		if (state == GAME_STATE_CAMPAIGN) {
			run_campaign();
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
