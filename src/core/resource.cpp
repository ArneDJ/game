#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <map>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "image.h"
#include "texture.h"
#include "resource.h"

TextureCache ResourceManager::texturecache;

const Texture* ResourceManager::load_texture(const std::string &fpath) 
{
	return texturecache.add(fpath);
}
