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
#include "core/texture.h"
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
	Shader shader;
	Camera camera;
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
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// load shaders
	shader.compile("shaders/debug.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	shader.link();

	float aspect_ratio = float(settings.window_width) / float(settings.window_height);
	camera.configure(0.1f, 9001.f, aspect_ratio, float(settings.FOV));
	camera.position = { 0.f, 0.f, 1.f };
	camera.lookat(glm::vec3(0.f, 0.f, 0.f));
	camera.project();

	SDL_SetRelativeMouseMode(SDL_FALSE);
}

void Game::teardown(void)
{
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

	const std::vector<glm::vec3> vertices = {
		{ -0.5f, -0.5f, 0.0f }, 
		{ 0.5f, -0.5f, 0.0f },
		{ -0.5f,  0.5f, 0.0f },
		{ 0.5f, -0.5f, 0.0f },
		{ -0.5f,  0.5f, 0.0f },
		{ 0.5f, 0.5f, 0.0f } 
	};
	const std::vector<glm::vec2> texcoords = {
		{ 0.f, 1.f },
		{ 1.f, 1.f },
		{ 0.f,  0.f },
		{ 1.f, 1.f },
		{ 0.f, 0.f },
		{ 1.,  0.f }
	};
	Mesh triangle = { vertices, texcoords };

	Image image = { "media/textures/pepper.png" };
	image.blur(10.f);
	Texture uncompressed = { &image };

	const Texture *compressed = textureman.add("media/textures/pepper.dds");

	while (running) {
		timer.begin();
		update();

		glClear(GL_COLOR_BUFFER_BIT);
		shader.use();
		shader.uniform_vec3("COLOR", glm::vec3(0.f, 1.f, 0.f));
		shader.uniform_mat4("VP", camera.VP);
		uncompressed.bind(GL_TEXTURE0);
		triangle.draw();

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
