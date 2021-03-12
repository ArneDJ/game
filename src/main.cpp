#include <iostream>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window *window = SDL_CreateWindow("An SDL2 window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL);

	// Check that the window was successfully created
	if (window == NULL) {
		// In the case that the window could not be made...
		printf("Could not create window: %s\n", SDL_GetError());
		return 1;
	}

	SDL_GLContext glcontext = SDL_GL_CreateContext(window);

	GLenum error = glewInit();
	if (error != GLEW_OK) {
		return 1;
	}

	glClearColor(1.f, 0.f, 1.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	// The window is open: could enter program loop here (see SDL_PollEvent())
	SDL_GL_SwapWindow(window);

	SDL_Delay(3000);  // Pause execution for 3000 milliseconds, for example

	SDL_GL_DeleteContext(glcontext);
	// Close and destroy the window
	SDL_DestroyWindow(window);

	// Clean up
	SDL_Quit();

	return 0;
}
