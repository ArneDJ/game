#include <string>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include "../extern/aixlog/aixlog.h"
#include "window.h"

using namespace CORE;
	
bool Window::open(uint16_t w, uint16_t h)
{
	width = w;
	height = h;

	SDL_Init(SDL_INIT_VIDEO);

	// open the SDL window
	window = SDL_CreateWindow("game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);
	if (window == nullptr) {
		LOG(ERROR, "Window") << "SDL Window could not be created!";
		return false;
	}

	// create OpenGL context
	glcontext = SDL_GL_CreateContext(window);
	if (glcontext == nullptr) {
		LOG(ERROR, "Window") << "SDL_GL context could not be created!";
		return false;
	}
	// set up GLEW
	GLenum error = glewInit();
	if (error != GLEW_OK) {
		LOG(ERROR, "Window") << "Could not initialize GLEW!";
		return false;
	}

	// VSYNC
	//if (winflags & VSYNC) { SDL_GL_SetSwapInterval(1); }

	return true;
}

void Window::close(void)
{
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);

	SDL_Quit();
}

void Window::swap(void) 
{ 
	SDL_GL_SwapWindow(window); 
};
	
void Window::set_fullscreen(void)
{
	SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
}
