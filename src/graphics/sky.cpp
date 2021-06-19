#include <iostream>
#include <random>
#include <vector>
#include <map>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../util/entity.h"
#include "../util/camera.h"
#include "shader.h"
#include "mesh.h"
#include "clouds.h"
#include "sky.h"

namespace GRAPHICS {

static const float DIST_MIN_COVERAGE = 0.F;
static const float DIST_MAX_COVERAGE = 0.8F;
static const float MIN_COVERAGE = 0.2F;

void Skybox::init(uint16_t width, uint16_t height)
{
	// create the cubemap mesh
	const std::vector<glm::vec3> positions = {
		{ -1.0, -1.0,  1.0 },
		{ 1.0, -1.0,  1.0 },
		{ 1.0,  1.0,  1.0 },
		{ -1.0,  1.0,  1.0 },
		{ -1.0, -1.0, -1.0 },
		{ 1.0, -1.0, -1.0 },
		{ 1.0,  1.0, -1.0 },
		{ -1.0,  1.0, -1.0 }
	};
	// indices
	const std::vector<uint16_t> indices = {
		2, 1, 0, 0, 3, 2,
		6, 5, 1, 1, 2, 6,
		5, 6, 7, 7, 4, 5,
		3, 0, 4, 4, 7, 3,
		1, 5, 4, 4, 0, 1,
		6, 2, 3, 3, 7, 6
	};

	cubemap = new Mesh { positions, indices };

	shader.compile("shaders/skybox.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/skybox.frag", GL_FRAGMENT_SHADER);
	shader.link();
	
	clouds.init(width, height);
}

void Skybox::teardown(void)
{
	clouds.teardown();
	delete cubemap;
}

void Skybox::prepare(void)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> coverage_dist(DIST_MIN_COVERAGE, DIST_MAX_COVERAGE);

	float coverage = coverage_dist(gen);
	clouds.gen_parameters(coverage);

	// if there are too few clouds simply disable them
	clouded = (coverage > MIN_COVERAGE);
}
	
void Skybox::colorize(const glm::vec3 &top, const glm::vec3 &bottom, const glm::vec3 &sunpos, const glm::vec3 &ambiance, bool cloudsenabled)
{
	zenith = top;
	horizon = bottom;
	sunposition = sunpos;
	clouds_enabled = cloudsenabled;
	m_ambiance = ambiance;
}

void Skybox::update(const UTIL::Camera *camera, float time)
{
	if (clouds_enabled && clouded) {
		clouds.update(camera, sunposition, time);
	}
}
	
void Skybox::display(const UTIL::Camera *camera) const
{
	shader.use();
	shader.uniform_vec3("ZENITH_COLOR", zenith);
	shader.uniform_vec3("HORIZON_COLOR", horizon);
	shader.uniform_vec3("AMBIANCE_COLOR", m_ambiance);
	shader.uniform_vec3("SUN_POS", sunposition);
	shader.uniform_bool("CLOUDS_ENABLED", clouds_enabled && clouded);
	shader.uniform_float("SCREEN_WIDTH", float(camera->width));
	shader.uniform_float("SCREEN_HEIGHT", float(camera->height));
	shader.uniform_mat4("V", camera->viewing);
	shader.uniform_mat4("P", camera->projection);
			
	clouds.bind(GL_TEXTURE0);

	glDepthFunc(GL_LEQUAL);
	cubemap->draw();
	glDepthFunc(GL_LESS);
}

};
