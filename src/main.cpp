#include <iostream>
#include <unordered_map>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>

#include "core/window.h"
#include "core/input.h"

class Game {
public:
	void run(void);
private:
	bool running;
	WindowManager windowman;
	InputManager inputman;
private:
	void input_event(const SDL_Event *event);
};

void Game::run(void)
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

	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) { input_event(&event); }
		inputman.update();

		glClear(GL_COLOR_BUFFER_BIT);

		windowman.swap();
	}

	windowman.teardown();
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
