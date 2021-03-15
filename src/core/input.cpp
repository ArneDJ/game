#include <unordered_map>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>

#include "input.h"
	
void InputManager::update(void) 
{ 
	//copy over keymap to previous keymap
	for (auto &iter : keymap) {
		previouskeys[iter.first] = iter.second;
	}

	int x, y;
	SDL_GetRelativeMouseState(&x, &y);
	mousecoords.relative.x = float(x);
	mousecoords.relative.y = float(y);
}

void InputManager::press_key(uint32_t keyID)
{
	keymap[keyID] = true;
}

void InputManager::release_key(uint32_t keyID)
{
	keymap[keyID] = false;
}

bool InputManager::key_down(uint32_t keyID) const
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

bool InputManager::key_pressed(uint32_t keyID) const
{
	return (key_down(keyID) && !was_key_down(keyID));
}

bool InputManager::was_key_down(uint32_t keyID) const
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

glm::vec2 InputManager::abs_mousecoords(void) const
{
	return mousecoords.absolute;
}

glm::vec2 InputManager::rel_mousecoords(void) const
{
	return mousecoords.relative;
}
