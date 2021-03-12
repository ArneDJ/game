#include <string>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include "logger.h"
#include "window.h"
	
bool WindowManager::init(uint16_t w, uint16_t h, uint32_t flags)
{
	width = w;
	height = h;

	SDL_Init(SDL_INIT_VIDEO);

	// open the SDL window
	window = SDL_CreateWindow("game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | flags);
	if (window == nullptr) {
		write_log(LogType::ERROR, std::string("SDL Window could not be created!"));
		return false;
	}

	// create OpenGL context
	glcontext = SDL_GL_CreateContext(window);
	if (glcontext == nullptr) {
		write_log(LogType::ERROR, std::string("SDL_GL context could not be created!"));
		return false;
	}
	// set up GLEW
	GLenum error = glewInit();
	if (error != GLEW_OK) {
		write_log(LogType::ERROR, std::string("Could not initialize glew!"));
		return false;
	}

	std::string glversion = (const char*)glGetString(GL_VERSION);
	write_log(LogType::RUN, "*** OpenGL Version: " + glversion + "***");

	// VSYNC
	//if (winflags & VSYNC) { SDL_GL_SetSwapInterval(1); }

	return true;
}

void WindowManager::teardown(void)
{
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);

	SDL_Quit();
}

void WindowManager::swap(void) 
{ 
	SDL_GL_SwapWindow(window); 
};
