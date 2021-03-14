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

#include "core/image.h"
#include "core/entity.h"
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
private:
	void init(void);
	void teardown(void);
	void update(void);
	void input_event(const SDL_Event *event);
};

void Game::init(void)
{
	running = true;

	if (!windowman.init(640, 480, 0)) {
		exit(EXIT_FAILURE);
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
}

void Game::teardown(void)
{
	windowman.teardown();
}

void Game::update(void)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) { input_event(&event); }
	inputman.update();
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
	image.blur(50.f);

	//const Texture *texture = textureman.add("media/textures/pepper.dds");
	Texture texture = { &image };

	while (running) {
		timer.begin();
		update();

		glClear(GL_COLOR_BUFFER_BIT);
		shader.use();
		shader.uniform_vec3("COLOR", glm::vec3(0.f, 1.f, 0.f));
		texture.bind(GL_TEXTURE0);
		triangle.draw();

		windowman.swap();
		timer.end();
	}
}

void Game::input_event(const SDL_Event *event)
{
	if (event->type == SDL_QUIT) { running = false; }

	if (event->type == SDL_MOUSEMOTION) {
		inputman.set_abs_mousecoords(float(event->motion.x), float(event->motion.y));
		inputman.set_rel_mousecoords(float(event->motion.xrel), float(event->motion.yrel));
	}

	if (event->type == SDL_KEYDOWN) {
		inputman.press_key(event->key.keysym.sym);
	}
	if (event->type == SDL_KEYUP) {
		inputman.release_key(event->key.keysym.sym);
	}

	if (event->type == SDL_MOUSEBUTTONDOWN) {
		inputman.press_key(event->button.button);
	}
	if (event->type == SDL_MOUSEBUTTONUP) {
		inputman.release_key(event->button.button);
	}
}

int main(int argc, char *argv[])
{
	Game game;
	game.run();
	/*
	Image image = { "media/textures/pepper.png" };
	printf("%d\n", image.rows());
	printf("%d\n", image.columns());
	image.blur(10.f);
	image.write("media/textures/altered.png");
	*/

	return 0;
}
