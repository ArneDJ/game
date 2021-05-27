#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
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
#include "mesh.h"
#include "model.h"
#include "media.h"

std::string MediaManager::basepath;
std::map<uint64_t, Texture*> MediaManager::textures;
std::map<uint64_t, GLTF::Model*> MediaManager::models;

static inline bool file_access(const std::string &filepath)
{
	std::ifstream file(filepath.c_str());

	return file.good();
}

void MediaManager::change_path(const std::string &path)
{
	basepath = path + "media/";
}

void MediaManager::teardown(void)
{
	// delete textures
	for (auto it = textures.begin(); it != textures.end(); it++) {
		Texture *texture = it->second;
		delete texture;
	}
	// delete models
	for (auto it = models.begin(); it != models.end(); it++) {
		GLTF::Model *model = it->second;
		delete model;
	}
}
	
const GLTF::Model* MediaManager::load_model(const std::string &relpath)
{
	std::string filepath = basepath + "models/" + relpath;
	uint64_t ID = std::hash<std::string>()(filepath);

	auto mit = models.find(ID);

	// model not found in map
	// load new one
	if (mit == models.end()) {
		GLTF::Model *model = new GLTF::Model { filepath };
		std::vector<const Texture*> materials;
		// load diffuse texture if present
		std::string diffusepath = relpath.substr(0, relpath.find_last_of('.')) + ".dds";
		if (file_access(basepath + "textures/" + diffusepath)) {
			materials.push_back(load_texture(diffusepath));
		}
		model->load_materials(materials);

		models.insert(std::make_pair(ID, model));

		return model;
	}

	return mit->second;
}

const Texture* MediaManager::load_texture(const std::string &relpath)
{
	std::string filepath = basepath + "textures/" + relpath;
	uint64_t ID = std::hash<std::string>()(filepath);

	auto mit = textures.find(ID);

	// texture not found in map
	// load new one
	if (mit == textures.end()) {
		Texture *texture = new Texture { filepath };
		textures.insert(std::make_pair(ID, texture));

		return texture;
	}

	return mit->second;
}
