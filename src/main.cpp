#include <iostream>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include "core/window.h"

int main(int argc, char *argv[])
{
	WindowManager windowman;
	if (!windowman.init(640, 480, SDL_WINDOW_BORDERLESS)) {
		exit(EXIT_FAILURE);
	}

	// set OpenGL states
	// TODO rendermanager should do this
	glClearColor(1.f, 0.f, 1.f, 1.f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClear(GL_COLOR_BUFFER_BIT);

	windowman.swap();

	SDL_Delay(3000);

	windowman.teardown();

	return 0;
}
