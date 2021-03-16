#include <iostream>
#include <unordered_map>
#include <chrono>
#include <map>
#include <vector>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/inih/INIReader.h"

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
//#include "core/sound.h" // TODO replace SDL_Mixer with OpenAL

class Game {
public:
	void run(void);
private:
	bool running;
	WindowManager windowman;
	InputManager inputman;
	TextureManager textureman;
	Timer timer;
	Shader object_shader;
	Shader debug_shader;
	Camera camera;
	Skybox *skybox;
	struct {
		uint16_t window_width;
		uint16_t window_height;
		bool fullscreen;
		int FOV;
		float look_sensitivity;
	} settings;

private:
	void init(void);
	void teardown(void);
	void update(void);
};

void Game::init(void)
{
	running = true;

	// load settings
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

	if (!windowman.init(settings.window_width, settings.window_height)) {
		exit(EXIT_FAILURE);
	}
	if (settings.fullscreen) {
		windowman.set_fullscreen();
	}

	// set OpenGL states
	// TODO rendermanager should do this
	glClearColor(1.f, 0.f, 1.f, 1.f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// load shaders
	debug_shader.compile("shaders/debug.vert", GL_VERTEX_SHADER);
	debug_shader.compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	debug_shader.link();

	object_shader.compile("shaders/object.vert", GL_VERTEX_SHADER);
	object_shader.compile("shaders/object.frag", GL_FRAGMENT_SHADER);
	object_shader.link();

	float aspect_ratio = float(settings.window_width) / float(settings.window_height);
	camera.configure(0.1f, 9001.f, aspect_ratio, float(settings.FOV));
	camera.position = { 0.f, 0.f, 5.f };
	camera.lookat(glm::vec3(0.f, 0.f, 0.f));
	camera.project();

	SDL_SetRelativeMouseMode(SDL_FALSE);

	skybox = new Skybox { glm::vec3(0.447f, 0.639f, 0.784f), glm::vec3(0.647f, 0.623f, 0.672f) };
}

void Game::teardown(void)
{
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
}

void Game::run(void)
{
	init();

	const std::vector<glm::vec3> positions = {
		{ -1.0, -1.0,  1.0 },
		{ 1.0, -1.0,  1.0 },
		{ 1.0,  1.0,  1.0 },
		{ -1.0,  1.0,  1.0 },
		{ -1.0, -1.0, -1.0 },
		{ 1.0, -1.0, -1.0 },
		{ 1.0,  1.0, -1.0 },
		{ -1.0,  1.0, -1.0 }
	};
	// indices
	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0,
		1, 5, 6, 6, 2, 1,
		7, 6, 5, 5, 4, 7,
		4, 0, 3, 3, 7, 4,
		4, 5, 1, 1, 0, 4,
		3, 2, 6, 6, 7, 3
	};
	//Mesh cube = { positions, indices };

	GLTF::Model dragon = { "media/models/dragon.glb", "" };
	GLTF::Model cube = { "media/models/cube.glb", "media/textures/cube.dds" };

	//const Texture *compressed = textureman.add("media/textures/cube.dds");

	while (running) {
		timer.begin();
		update();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		debug_shader.use();
		debug_shader.uniform_vec3("COLOR", glm::vec3(0.f, 1.f, 0.f));
		debug_shader.uniform_mat4("VP", camera.VP);
		debug_shader.uniform_mat4("MODEL", glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f)));
		//cube.draw();
		dragon.display();
		object_shader.use();
		object_shader.uniform_mat4("VP", camera.VP);
		object_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), glm::vec3(10.f, 0.f, 0.f)));
		cube.display();

		skybox->display(&camera);

		windowman.swap();
		timer.end();
	}
}

int main(int argc, char *argv[])
{
	Game game;
	game.run();

	return 0;
}
