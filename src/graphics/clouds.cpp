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

#include "../core/shader.h"
#include "../core/image.h"
#include "../core/texture.h"
#include "../core/camera.h"
#include "clouds.h"

#define INT_CEIL(n,d) (int)ceil((float)n/d)

static const float EARTH_RADIUS = 600000.F;
static const float CURLINESS = 0.1F;
static const float ABSORPTION = 0.0035F;
// cloud parameters
static const float MIN_SPEED = 200.F;
static const float MAX_SPEED = 300.F;
static const float MIN_COVERAGE = 0.3F;
static const float MAX_COVERAGE = 0.8F;
static const float MIN_CURLINESS = 0.F;
static const float MAX_CURLINESS = 3.F;
static const float MIN_CRISPINESS = 30.F;
static const float MAX_CRISPINESS = 50.F;
static const float MIN_DENSITY = 0.F;
static const float MAX_DENSITY = 0.1F;
static const size_t PERLIN_RES = 128;
static const size_t WORLEY_RES = 32;
static const size_t WEATHERMAP_RES = 1024;

static GLuint generateTexture2D(int w, int h);
static GLuint generateTexture3D(int w, int h, int d);

void Clouds::init(void)
{
	inner_radius = 2000.F;
	outer_radius = 14000.F;

	perlin_frequency = 0.8F;

	powdered = false;

	weathermap = 0;
	perlin = 0;
	worley32 = 0;

	gen_parameters();

	init_shaders();
	
	perlin = generateTexture3D(PERLIN_RES, PERLIN_RES, PERLIN_RES);
	worley32 = generateTexture3D(WORLEY_RES, WORLEY_RES, WORLEY_RES);
	weathermap = generateTexture2D(WEATHERMAP_RES, WEATHERMAP_RES);

	generate_textures();
	
	generate_weather();
}

void Clouds::teardown(void)
{
	if (glIsTexture(weathermap) == GL_TRUE) {
		glDeleteTextures(1, &weathermap);
	}
	if (glIsTexture(perlin) == GL_TRUE) {
		glDeleteTextures(1, &perlin);
	}
	if (glIsTexture(worley32) == GL_TRUE) {
		glDeleteTextures(1, &worley32);
	}
}

void Clouds::gen_parameters(void)
{
	std::random_device rd;
	std::mt19937 gen(rd());

	std::uniform_real_distribution<float> speed_dist(MIN_SPEED, MAX_SPEED);
	speed = speed_dist(gen);
	std::uniform_real_distribution<float> coverage_dist(MIN_COVERAGE, MAX_COVERAGE);
	coverage = coverage_dist(gen);
	std::uniform_real_distribution<float> crispiness_dist(MIN_CRISPINESS, MAX_CRISPINESS);
	crispiness = crispiness_dist(gen);

	std::uniform_real_distribution<float> density_dist(MIN_DENSITY, MAX_DENSITY);
	density = density_dist(gen);

	printf("speed %f\n", speed);
	printf("coverage %f\n", coverage);
	printf("crispiness %f\n", crispiness);
	printf("density %f\n", density);

	std::uniform_real_distribution<float> dist;
	seed = { dist(gen), dist(gen), dist(gen) };

	topcolor = (glm::vec3(169., 149., 149.)*(1.5f / 255.f));
	bottomcolor = (glm::vec3(65., 70., 80.)*(1.5f / 255.f));
}

void Clouds::init_shaders(void)
{
	volumetric.compile("shaders/clouds.comp", GL_COMPUTE_SHADER);
	volumetric.link();

	weather.compile("shaders/weather.comp", GL_COMPUTE_SHADER);
	weather.link();

	perlinworley.compile("shaders/perlinworley.comp", GL_COMPUTE_SHADER);
	perlinworley.link();

	worley.compile("shaders/worley.comp", GL_COMPUTE_SHADER);
	worley.link();
}

void Clouds::generate_weather(void) 
{
	glBindImageTexture(0, weathermap, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	weather.use();
	weather.uniform_vec3("SEED", seed);
	weather.uniform_float("PERLIN_FREQ", perlin_frequency);

	glDispatchCompute(INT_CEIL(WEATHERMAP_RES, 8), INT_CEIL(WEATHERMAP_RES, 8), 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
	
void Clouds::generate_textures(void)
{
	// create the perlin worley noise mix texture
	
	perlinworley.use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, perlin);
	glBindImageTexture(0, perlin, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glDispatchCompute(INT_CEIL(PERLIN_RES, 4), INT_CEIL(PERLIN_RES, 4), INT_CEIL(PERLIN_RES, 4));
	//glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glGenerateMipmap(GL_TEXTURE_3D);

	worley.use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, worley32);
	glBindImageTexture(0, worley32, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glDispatchCompute(INT_CEIL(WORLEY_RES, 4), INT_CEIL(WORLEY_RES, 4), INT_CEIL(WORLEY_RES, 4));
	glGenerateMipmap(GL_TEXTURE_3D);

	// create the weather map
	glBindImageTexture(0, weathermap, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	weather.use();
	weather.uniform_vec3("SEED", seed);
	glDispatchCompute(INT_CEIL(WEATHERMAP_RES, 8), INT_CEIL(WEATHERMAP_RES, 8), 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void Cloudscape::init(int SW, int SH)
{
	clouds.init();

	SCR_WIDTH = SW;
	SCR_HEIGHT = SH;
	cloudscape = generateTexture2D(SW, SH);
	blurred_cloudscape = generateTexture2D(SW, SH);

	glBindTexture(GL_TEXTURE_2D, cloudscape);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, blurred_cloudscape);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	blur_shader.compile("shaders/blur.comp", GL_COMPUTE_SHADER);
	blur_shader.link();
}

void Cloudscape::teardown(void)
{
	clouds.teardown();
	if (glIsTexture(cloudscape) == GL_TRUE) {
		glDeleteTextures(1, &cloudscape);
	}
	if (glIsTexture(blurred_cloudscape) == GL_TRUE) {
		glDeleteTextures(1, &blurred_cloudscape);
	}
}

void Cloudscape::update(const Camera *camera, const glm::vec3 &lightpos, float time)
{
	Shader &volumetric = clouds.volumetric;
	volumetric.use();

	volumetric.uniform_vec2("RESOLUTION", glm::vec2(SCR_WIDTH, SCR_HEIGHT));
	volumetric.uniform_float("TIME", time);
	volumetric.uniform_mat4("INV_PROJ", glm::inverse(camera->projection));
	volumetric.uniform_mat4("INV_VIEW", glm::inverse(camera->viewing));
	volumetric.uniform_vec3("CAM_POS", camera->position);
	volumetric.uniform_float("FOV", camera->FOV);
	volumetric.uniform_vec3("SUN_POS", glm::normalize(lightpos));
	volumetric.uniform_vec3("LIGHT_COLOR", glm::vec3(1.f, 1.f, 1.f));
	
	volumetric.uniform_float("COVERAGE", clouds.coverage);
	volumetric.uniform_float("SPEED", clouds.speed);
	volumetric.uniform_float("CRISPINESS", clouds.crispiness);
	volumetric.uniform_float("CURLINESS", CURLINESS);
	volumetric.uniform_float("ABSORPTION", ABSORPTION);
	volumetric.uniform_float("DENSITY_FACTOR", clouds.density);

	volumetric.uniform_bool("ENABLE_POWDER", clouds.powdered);

	volumetric.uniform_float("EARTH_RADIUS", EARTH_RADIUS);
	volumetric.uniform_float("INNER_RADIUS", clouds.inner_radius);
	volumetric.uniform_float("OUTER_RADIUS", clouds.outer_radius);

	volumetric.uniform_vec3("COLOR_TOP", clouds.topcolor);
	volumetric.uniform_vec3("COLOR_BOTTOM", clouds.bottomcolor);

	glBindImageTexture(0, cloudscape, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_3D, clouds.perlin);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_3D, clouds.worley32);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, clouds.weathermap);

	glDispatchCompute(INT_CEIL(SCR_WIDTH, 16), INT_CEIL(SCR_HEIGHT, 16), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// now blur the final cloud texture
	glBindImageTexture(0, cloudscape, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindImageTexture(1, blurred_cloudscape, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	blur_shader.use();

	blur_shader.uniform_vec2("DIR", glm::vec2(1.f, 0.f));

	glDispatchCompute(INT_CEIL(SCR_WIDTH, 16), INT_CEIL(SCR_WIDTH, 16), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	blur_shader.uniform_vec2("DIR", glm::vec2(0.f, 1.f));

	glDispatchCompute(INT_CEIL(SCR_WIDTH, 16), INT_CEIL(SCR_WIDTH, 16), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

// TODO put this in texture class
static GLuint generateTexture3D(int w, int h, int d) 
{
	GLuint tex_output;
	glGenTextures(1, &tex_output);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, tex_output);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexStorage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, w, h, d);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, w, h, d, 0, GL_RGBA, GL_FLOAT, NULL);
	glGenerateMipmap(GL_TEXTURE_3D);
	glBindImageTexture(0, tex_output, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

	return tex_output;
}

// TODO put this in texture class
static GLuint generateTexture2D(int w, int h) 
{
	GLuint tex_output;
	glGenTextures(1, &tex_output);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_output);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);

	glBindImageTexture(0, tex_output, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	return tex_output;
}
