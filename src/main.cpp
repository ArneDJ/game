#include <iostream>
#include <unordered_map>
#include <chrono>
#include <map>
#include <vector>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>

#include "core/window.h"
#include "core/input.h"
#include "core/timer.h"
#include "core/shader.h"
//#include "core/sound.h" // TODO replace SDL_Mixer with OpenAL

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

class Game {
public:
	void run(void);
private:
	bool running;
	WindowManager windowman;
	InputManager inputman;
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

	GLuint VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	const GLfloat vertices[] = {
		-0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		0.0f,  0.5f, 0.0f
	};
	GLuint VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0)); 
	glEnableVertexAttribArray(0);

	while (running) {
		timer.begin();
		update();

		glClear(GL_COLOR_BUFFER_BIT);
		shader.use();
		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		windowman.swap();
		timer.end();
	}

	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);
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

	return 0;
}
