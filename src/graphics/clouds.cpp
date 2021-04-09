#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <map>
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
#include "../core/buffers.h"
#include "../core/camera.h"
#include "clouds.h"

#define INT_CEIL(n,d) (int)ceil((float)n/d)


void Clouds::init(void)
{
	init_variables();
	init_shaders();
	generate_textures();
	
	generate_weather();
}

Clouds::~Clouds(void)
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

void Clouds::init_variables(void)
{
	speed = 450.0;
	coverage = 0.45;
	crispiness = 40.;
	curliness = .1;
	density = 0.02;
	absorption = 0.35;

	earth_radius = 600000.0;
	sphere_inner_radius = 5000.0;
	sphere_outer_radius = 17000.0;

	perlin_frequency = 0.8;

	godrayed = false;
	powdered = false;

	seed = glm::vec3(0.0, 0.0, 0.0);
	old_seed = glm::vec3(0.0, 0.0, 0.0);

	topcolor = (glm::vec3(169., 149., 149.)*(1.5f / 255.f));
	bottomcolor = (glm::vec3(65., 70., 80.)*(1.5f / 255.f));

	weathermap = 0;
	perlin = 0;
	worley32 = 0;
}

void Clouds::init_shaders(void)
{
	volumetric.compile("shaders/clouds.comp", GL_COMPUTE_SHADER);
	volumetric.link();

	weather.compile("shaders/weather.comp", GL_COMPUTE_SHADER);
	weather.link();
}

void Clouds::generate_weather(void) 
{
	bindTexture2D(weathermap, 0);
	weather.use();
	weather.uniform_vec3("seed", seed);
	weather.uniform_float("perlinFrequency", perlin_frequency);
	std::cout << "computing weather!" << std::endl;
	glDispatchCompute(INT_CEIL(1024, 8), INT_CEIL(1024, 8), 1);
	std::cout << "weather computed!!" << std::endl;

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
	
void Clouds::generate_textures(void)
{
	// create the perlin worley noise mix texture
	// TODO shader check
	Shader comp;
	comp.compile("shaders/perlinworley.comp", GL_COMPUTE_SHADER);
	comp.link();
	
	// perlin
	//make texture
	perlin = generateTexture3D(128, 128, 128);
	//compute
	comp.use();
	comp.uniform_vec3("u_resolution", glm::vec3(128, 128, 128));
	std::cout << "computing perlinworley!" << std::endl;
	glActiveTexture(GL_TEXTURE0);
	comp.uniform_int("outVolTex", 0);
	glBindTexture(GL_TEXTURE_3D, perlin);
	glBindImageTexture(0, perlin, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glDispatchCompute(INT_CEIL(128, 4), INT_CEIL(128, 4), INT_CEIL(128, 4));
	std::cout << "computed!!" << std::endl;
	//glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glGenerateMipmap(GL_TEXTURE_3D);

	// worley
	//compute shaders
	Shader worley;
	worley.compile("shaders/worley.comp", GL_COMPUTE_SHADER);
	worley.link();

	//make texture
	worley32 = generateTexture3D(32, 32, 32);

	//compute
	worley.use();
	worley.uniform_vec3("u_resolution", glm::vec3(32, 32, 32));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, worley32);
	glBindImageTexture(0, worley32, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	std::cout << "computing worley 32!" << std::endl;
	glDispatchCompute(INT_CEIL(32, 4), INT_CEIL(32, 4), INT_CEIL(32, 4));
	std::cout << "computed!!" << std::endl;
	glGenerateMipmap(GL_TEXTURE_3D);

	// weathermap
	weathermap = generateTexture2D(1024, 1024);

	//compute
	glBindImageTexture(0, weathermap, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	weather.use();
	weather.uniform_vec3("seed", seed);
	weather.uniform_float("perlinFrequency", perlin_frequency);
	std::cout << "computing weather!" << std::endl;
	glDispatchCompute(INT_CEIL(1024, 8), INT_CEIL(1024, 8), 1);
	std::cout << "weather computed!!" << std::endl;

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// TODO seed
	//seed = scene->seed;
	old_seed = seed;
}

void Cloudscape::init(int SW, int SH)
{
	clouds.init();

	SCR_WIDTH = SW;
	SCR_HEIGHT = SH;
	cloudsFBO = new TextureSet(SW, SH, 4);
}

Cloudscape::~Cloudscape(void)
{
	delete cloudsFBO;
}

void Cloudscape::update(const Camera *camera, const glm::vec3 &lightpos, const glm::vec3 &zenith_color, const glm::vec3 &horizon_color, float time)
{
	float t1, t2;

	// attach textures where the final result goes
	for (int i = 0; i < cloudsFBO->getNTextures(); ++i) {
		bindTexture2D(cloudsFBO->getColorAttachmentTex(i), i);
	}

	Shader &cloudsShader = clouds.volumetric;
	cloudsShader.use();

	cloudsShader.uniform_vec2("iResolution", glm::vec2(SCR_WIDTH, SCR_HEIGHT));
	cloudsShader.uniform_float("iTime", time);
	cloudsShader.uniform_mat4("inv_proj", glm::inverse(camera->projection));
	cloudsShader.uniform_mat4("inv_view", glm::inverse(camera->viewing));
	cloudsShader.uniform_vec3("cameraPosition", camera->position);
	cloudsShader.uniform_float("FOV", camera->FOV);
	cloudsShader.uniform_vec3("lightDirection", glm::normalize(lightpos));
	cloudsShader.uniform_vec3("lightColor", glm::vec3(1.f, 1.f, 1.f));
	
	cloudsShader.uniform_float("coverage_multiplier", clouds.coverage);
	cloudsShader.uniform_float("cloudSpeed", clouds.speed);
	cloudsShader.uniform_float("crispiness", clouds.crispiness);
	cloudsShader.uniform_float("curliness", clouds.curliness);
	cloudsShader.uniform_float("absorption", clouds.absorption*0.01);
	cloudsShader.uniform_float("densityFactor", clouds.density);

	cloudsShader.uniform_bool("enablePowder", clouds.powdered);

	cloudsShader.uniform_float("earthRadius", clouds.earth_radius);
	cloudsShader.uniform_float("sphereInnerRadius", clouds.sphere_inner_radius);
	cloudsShader.uniform_float("sphereOuterRadius", clouds.sphere_outer_radius);

	cloudsShader.uniform_vec3("cloudColorTop", clouds.topcolor);
	cloudsShader.uniform_vec3("cloudColorBottom", clouds.bottomcolor);

	cloudsShader.uniform_vec3("skyColorTop", zenith_color);
	cloudsShader.uniform_vec3("skyColorBottom", horizon_color);

	glm::mat4 vp = camera->projection * camera->viewing;
	cloudsShader.uniform_mat4("invViewProj", glm::inverse(vp));
	cloudsShader.uniform_mat4("gVP", vp);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_3D, clouds.perlin);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_3D, clouds.worley32);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, clouds.weathermap);
	// isn't used
	//cloudsShader.setSampler2D("depthMap", s->sceneFBO->depthTex, 3);
	//glActiveTexture(GL_TEXTURE7);
	//glBindTexture(GL_TEXTURE_2D, sceneFBO->depthTex);

	//actual update
	glDispatchCompute(INT_CEIL(SCR_WIDTH, 16), INT_CEIL(SCR_HEIGHT, 16), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

