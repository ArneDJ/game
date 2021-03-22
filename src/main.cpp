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

//#include "extern/fastnoise/FastNoise.h"

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
//#include "core/sound.h" // TODO replace SDL_Mixer with OpenAL

class Game {
public:
	void run(void);
private:
	bool running;
	bool debugmode;
	WindowManager windowman;
	InputManager inputman;
	RenderManager renderman;
	PhysicsManager physicsman;
	Timer timer;
	Shader object_shader;
	Shader debug_shader;
	Camera camera;
	Skybox *skybox;
	Navigation navigation;
	Debugger debugger;
	struct {
		uint16_t window_width;
		uint16_t window_height;
		bool fullscreen;
		int FOV;
		float look_sensitivity;
	} settings;
	bool change_endpoint = false;
	glm::vec3 endpoint = {};
private:
	void init(void);
	void init_settings(void);
	void load_scene(void);
	void clear_scene(void);
	void update(void);
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
	running = true;

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

	// load shaders
	debug_shader.compile("shaders/debug.vert", GL_VERTEX_SHADER);
	debug_shader.compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	debug_shader.link();

	object_shader.compile("shaders/object.vert", GL_VERTEX_SHADER);
	object_shader.compile("shaders/object.frag", GL_FRAGMENT_SHADER);
	object_shader.link();

	camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	camera.position = { 10.f, 5.f, -10.f };
	camera.lookat(glm::vec3(0.f, 0.f, 0.f));
	camera.project();

	skybox = new Skybox { glm::vec3(0.447f, 0.639f, 0.784f), glm::vec3(0.647f, 0.623f, 0.672f) };

	if (debugmode) {
		debugger.init(windowman.window, windowman.glcontext);
	}
}

void Game::clear_scene(void)
{
	// then clear the physics manager
	physicsman.clear();
}

void Game::teardown(void)
{
	if (debugmode) {
		debugger.teardown();
	}

	delete skybox;

	windowman.teardown();
}

void Game::update(void)
{
	inputman.update();
	if (inputman.exit_request()) {
		running = false;
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
		debugger.update(timer.ms_per_frame, camera.position);
		if (debugger.exit_request) { running = false; }
	}

	inputman.update_keymap();
}

void Game::run(void)
{
	init();
	
	debugger.add_grid(glm::vec2(-20.f, -20.f), glm::vec2(20.f, 20.f));

	GLTF::Model duck = { "media/models/duck.glb", "media/textures/duck.dds" };
	GLTF::Model dragon = { "media/models/dragon.glb", "" };

	FastNoise fastnoise;
	fastnoise.SetSeed(1337);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(0.001f);
	fastnoise.SetPerturbFrequency(0.001f);
	fastnoise.SetFractalOctaves(6);
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetGradientPerturbAmp(200.f);
	Image image = { 512, 512, 1 };
	auto start = std::chrono::steady_clock::now();
	image.noise(&fastnoise, glm::vec2(4.f, 4.f), glm::vec2(0.f, 0.f), CHANNEL_RED);
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
	image.write("media/textures/noise.png");

	while (running) {
		timer.begin();

		update();

		renderman.prepare_to_render();

		debug_shader.use();
		debug_shader.uniform_vec3("COLOR", glm::vec3(0.f, 1.f, 0.f));
		debug_shader.uniform_mat4("VP", camera.VP);
		debug_shader.uniform_mat4("MODEL", glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f)));
		dragon.display();

		if (debugmode) {
			debug_shader.uniform_mat4("MODEL", glm::mat4(1.f));
			debugger.render_grids();
			debugger.render_navmeshes();
		}
	
		object_shader.use();
		object_shader.uniform_mat4("VP", camera.VP);
		object_shader.uniform_bool("INSTANCED", false);

		object_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), glm::vec3(10.f, 0.f, 10.f)));
		duck.display();

		skybox->display(&camera);

		if (debugmode) {
			debugger.render_GUI();
		}

		windowman.swap();
		timer.end();
	}

	clear_scene();
}

int main(int argc, char *argv[])
{
	write_start_log();

	Game game;
	game.run();

	write_exit_log();

	return 0;
}
