#include <unordered_map>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>

#include "input.h"
	
using namespace CORE;

Input::Input(void)
{
	exit = false;
	mousegrab = false;
	mousecoords.absolute = { 0.f, 0.f };
	mousecoords.relative = { 0.f, 0.f };
}
	
bool Input::exit_request(void) const
{
	return exit;
}

void Input::update(void) 
{ 
	mousewheel = 0;

	SDL_Event event;
	while (SDL_PollEvent(&event)) { sample_event(&event); }

	if (key_pressed(SDLK_TAB)) {
		mousegrab = !mousegrab;
		if (mousegrab) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
		} else {
			SDL_SetRelativeMouseMode(SDL_FALSE);
		}
	}

	sample_absolute_mousecoords();
	sample_relative_mousecoords();
}

void Input::update_keymap(void) 
{
	// copy over keymap of current tick to previous keymap
	for (auto &iter : keymap) {
		previouskeys[iter.first] = iter.second;
	}
}

void Input::sample_event(const SDL_Event *event)
{
	if (event->type == SDL_QUIT) { exit = true; }

	if (event->type == SDL_KEYDOWN) {
		press_key(event->key.keysym.sym);
	}
	if (event->type == SDL_KEYUP) {
		release_key(event->key.keysym.sym);
	}

	if (event->type == SDL_MOUSEBUTTONDOWN) {
		press_key(event->button.button);
	}
	if (event->type == SDL_MOUSEBUTTONUP) {
		release_key(event->button.button);
	}

	if (event->type == SDL_MOUSEWHEEL) {
		mousewheel = event->wheel.y;
	}
}

void Input::sample_relative_mousecoords(void) 
{
	int x = 0;
	int y = 0;
	SDL_GetRelativeMouseState(&x, &y);
	mousecoords.relative.x = float(x);
	mousecoords.relative.y = float(y);
	if (mousegrab == false) {
		mousecoords.relative.x = 0.f;
		mousecoords.relative.y = 0.f;
	}
}

void Input::sample_absolute_mousecoords(void) 
{
	int x = 0;
	int y = 0;
	SDL_GetMouseState(&x, &y);
	mousecoords.absolute.x = float(x);
	mousecoords.absolute.y = float(y);
}

void Input::press_key(uint32_t keyID)
{
	keymap[keyID] = true;
}

void Input::release_key(uint32_t keyID)
{
	keymap[keyID] = false;
}

bool Input::key_down(uint32_t keyID) const
{
	// We dont want to use the associative array approach here
	// because we don't want to create a key if it doesnt exist.
	// So we do it manually
	auto key = keymap.find(keyID);
	if (key != keymap.end()) {
		return key->second;   // Found the key
	}

	return false;
}

bool Input::key_pressed(uint32_t keyID) const
{
	return (key_down(keyID) && !was_key_down(keyID));
}

bool Input::was_key_down(uint32_t keyID) const
{
	// We dont want to use the associative array approach here
	// because we don't want to create a key if it doesnt exist.
	// So we do it manually
	auto key = previouskeys.find(keyID);
	if (key != previouskeys.end()) {
		return key->second;   // Found the key
	}

	return false;
}

glm::vec2 Input::abs_mousecoords(void) const
{
	return mousecoords.absolute;
}

glm::vec2 Input::rel_mousecoords(void) const
{
	return mousecoords.relative;
}

int Input::mousewheel_y(void) const
{
	return mousewheel;
}
