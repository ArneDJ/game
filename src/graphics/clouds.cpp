#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <random>
#include <GL/glew.h>
#include <GL/gl.h> 

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geom.h"
#include "../util/entity.h"
#include "../util/image.h"
#include "../util/camera.h"
#include "shader.h"
#include "texture.h"
#include "clouds.h"

namespace gfx {

#define INT_CEIL(n,d) (int)ceil((float)n/d)

static const float EARTH_RADIUS = 600000.F;
static const float INNER_RADIUS = 2000.F;
static const float OUTER_RADIUS = 14000.F;
static const float CURLINESS = 0.1F;
static const float CRISPINESS = 40.F;
static const float ABSORPTION = 0.0035F;
static const float DENSITY = 0.02F;
static const float PERLIN_FREQUENCY = 0.8F;
static const size_t PERLIN_RES = 128;
static const size_t WORLEY_RES = 32;
static const size_t WEATHERMAP_RES = 1024;

// cloud parameters
static const float MIN_SPEED = 100.F;
static const float MAX_SPEED = 200.F;

void Clouds::init(int SW, int SH)
{
	m_screen_width = SW;
	m_screen_height = SH;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist;
	m_seed = { dist(gen), dist(gen), dist(gen) };

	m_top_color = (glm::vec3(169., 149., 149.)*(1.5f / 255.f));
	m_bottom_color = (glm::vec3(65., 70., 80.)*(1.5f / 255.f));

	m_weathermap = 0;

	m_perlin = 0;
	m_worley32 = 0;

	gen_parameters(1.f);

	init_shaders();
	
	m_perlin = generate_3D_texture(PERLIN_RES, PERLIN_RES, PERLIN_RES, GL_RGBA8, GL_RGBA, GL_FLOAT);
	m_worley32 = generate_3D_texture(WORLEY_RES, WORLEY_RES, WORLEY_RES, GL_RGBA8, GL_RGBA, GL_FLOAT);
	m_weathermap = generate_2D_texture(NULL, WEATHERMAP_RES, WEATHERMAP_RES, GL_RGBA32F, GL_RGBA, GL_FLOAT);

	generate_noise();

	m_cloudscape = generate_2D_texture(NULL, SW, SH, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	m_blurred_cloudscape = generate_2D_texture(NULL, SW, SH, GL_RGBA32F, GL_RGBA, GL_FLOAT);

	glBindTexture(GL_TEXTURE_2D, m_cloudscape);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, m_blurred_cloudscape);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Clouds::teardown()
{
	delete_texture(m_weathermap);
	delete_texture(m_perlin);
	delete_texture(m_worley32);
	delete_texture(m_cloudscape);
	delete_texture(m_blurred_cloudscape);
}

void Clouds::gen_parameters(float covrg)
{
	m_coverage = covrg;

	// random parameters
	std::random_device rd;
	std::mt19937 gen(rd());

	std::uniform_real_distribution<float> speed_dist(MIN_SPEED, MAX_SPEED);
	m_speed = speed_dist(gen);

	std::uniform_real_distribution<float> wind_dist(-1.f, 1.f);
	m_wind_direction = glm::normalize(glm::vec3(wind_dist(gen), 0.f, wind_dist(gen)));
}

void Clouds::init_shaders()
{
	m_perlinworley.compile("shaders/perlinworley.comp", GL_COMPUTE_SHADER);
	m_perlinworley.link();

	m_worley.compile("shaders/worley.comp", GL_COMPUTE_SHADER);
	m_worley.link();

	m_weather.compile("shaders/weather.comp", GL_COMPUTE_SHADER);
	m_weather.link();

	m_volumetric.compile("shaders/clouds.comp", GL_COMPUTE_SHADER);
	m_volumetric.link();

	m_blurry.compile("shaders/blur.comp", GL_COMPUTE_SHADER);
	m_blurry.link();
}
	
void Clouds::generate_noise()
{
	// create the perlin worley noise mix texture
	m_perlinworley.use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, m_perlin);
	glBindImageTexture(0, m_perlin, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glDispatchCompute(INT_CEIL(PERLIN_RES, 4), INT_CEIL(PERLIN_RES, 4), INT_CEIL(PERLIN_RES, 4));
	glGenerateMipmap(GL_TEXTURE_3D);

	m_worley.use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, m_worley32);
	glBindImageTexture(0, m_worley32, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glDispatchCompute(INT_CEIL(WORLEY_RES, 4), INT_CEIL(WORLEY_RES, 4), INT_CEIL(WORLEY_RES, 4));
	glGenerateMipmap(GL_TEXTURE_3D);

	// create the weather map
	glBindImageTexture(0, m_weathermap, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	m_weather.use();
	m_weather.uniform_vec3("SEED", m_seed);
	m_weather.uniform_float("PERLIN_FREQ", PERLIN_FREQUENCY);
	glDispatchCompute(INT_CEIL(WEATHERMAP_RES, 8), INT_CEIL(WEATHERMAP_RES, 8), 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void Clouds::update(const util::Camera *camera, const glm::vec3 &lightpos, float time)
{
	m_volumetric.use();

	m_volumetric.uniform_vec2("RESOLUTION", glm::vec2(m_screen_width, m_screen_height));
	m_volumetric.uniform_float("TIME", time);
	m_volumetric.uniform_mat4("INV_PROJ", glm::inverse(camera->projection));
	m_volumetric.uniform_mat4("INV_VIEW", glm::inverse(camera->viewing));
	m_volumetric.uniform_vec3("CAM_POS", camera->position);
	m_volumetric.uniform_float("FOV", camera->FOV);
	m_volumetric.uniform_vec3("SUN_POS", glm::normalize(lightpos));
	m_volumetric.uniform_vec3("LIGHT_COLOR", glm::vec3(1.f, 1.f, 1.f));
	
	m_volumetric.uniform_vec3("WIND_DIRECTION", m_wind_direction);
	m_volumetric.uniform_float("COVERAGE", m_coverage);
	m_volumetric.uniform_float("SPEED", m_speed);
	m_volumetric.uniform_float("CRISPINESS", CRISPINESS);
	m_volumetric.uniform_float("CURLINESS", CURLINESS);
	m_volumetric.uniform_float("ABSORPTION", ABSORPTION);
	m_volumetric.uniform_float("DENSITY_FACTOR", DENSITY);

	m_volumetric.uniform_bool("ENABLE_POWDER", false);

	m_volumetric.uniform_float("EARTH_RADIUS", EARTH_RADIUS);
	m_volumetric.uniform_float("INNER_RADIUS", INNER_RADIUS);
	m_volumetric.uniform_float("OUTER_RADIUS", OUTER_RADIUS);

	m_volumetric.uniform_vec3("COLOR_TOP", m_top_color);
	m_volumetric.uniform_vec3("COLOR_BOTTOM", m_bottom_color);

	glBindImageTexture(0, m_cloudscape, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_3D, m_perlin);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_3D, m_worley32);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, m_weathermap);

	glDispatchCompute(INT_CEIL(m_screen_width, 16), INT_CEIL(m_screen_height, 16), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// now blur the final cloud texture
	glBindImageTexture(0, m_cloudscape, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(1, m_blurred_cloudscape, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	m_blurry.use();

	m_blurry.uniform_vec2("DIR", glm::vec2(1.f, 0.f));

	glDispatchCompute(INT_CEIL(m_screen_width, 16), INT_CEIL(m_screen_height, 16), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	m_blurry.uniform_vec2("DIR", glm::vec2(0.f, 1.f));

	glDispatchCompute(INT_CEIL(m_screen_width, 16), INT_CEIL(m_screen_height, 16), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void Clouds::bind(GLenum unit) const
{
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D, m_blurred_cloudscape);
}

};
