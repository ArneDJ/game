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


Clouds::Clouds(void)
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
	postprocessed = true;

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
	volumetric.compile("shaders/volumetric_clouds.comp", GL_COMPUTE_SHADER);
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
	//bindTexture2D(weathermap, 0);
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

Cloudscape::Cloudscape(int SW, int SH)
{
	SCR_WIDTH = SW;
	SCR_HEIGHT = SH;
	cloudsFBO = new TextureSet(SW, SH, 4);
	cloudsPostProcessingFBO = new FrameBufferObject(1920, 1080, 2);
}

Cloudscape::~Cloudscape(void)
{
	delete cloudsFBO;
	delete cloudsPostProcessingFBO;
}

void Cloudscape::draw(const Camera *camera, const glm::vec3 &lightpos, const glm::vec3 &zenith_color, const glm::vec3 &horizon_color)
{
	float t1, t2;

	//cloudsFBO->bind();
	// attach textures where the final result goes
	for (int i = 0; i < cloudsFBO->getNTextures(); ++i) {
		bindTexture2D(cloudsFBO->getColorAttachmentTex(i), i);
	}

	Shader &cloudsShader = clouds.volumetric;
//sceneElements* s = drawableObject::scene;
	cloudsShader.use();

	cloudsShader.uniform_vec2("iResolution", glm::vec2(SCR_WIDTH, SCR_HEIGHT));
	cloudsShader.uniform_float("iTime", 1.f);
	cloudsShader.uniform_mat4("inv_proj", glm::inverse(camera->projection));
	cloudsShader.uniform_mat4("inv_view", glm::inverse(camera->viewing));
	cloudsShader.uniform_vec3("cameraPosition", camera->position);
	cloudsShader.uniform_float("FOV", camera->FOV);
	cloudsShader.uniform_vec3("lightDirection", glm::normalize(lightpos - camera->position));
	cloudsShader.uniform_vec3("lightColor", glm::vec3(1.f, 1.f, 1.f));
	
	cloudsShader.uniform_float("coverage_multiplier", clouds.coverage);
	cloudsShader.uniform_float("cloudSpeed", clouds.speed);
	cloudsShader.uniform_float("crispiness", clouds.crispiness);
	cloudsShader.uniform_float("curliness", clouds.curliness);
	cloudsShader.uniform_float("absorption", clouds.absorption*0.01);
	cloudsShader.uniform_float("densityFactor", clouds.density);

	//cloudsShader.setBool("enablePowder", enablePowder);

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

	//cloudsShader.setSampler3D("cloud", clouds.perlin, 0);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_3D, clouds.perlin);
	//cloudsShader.setSampler3D("worley32", clouds.worley32, 1);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_3D, clouds.worley32);
	//cloudsShader.setSampler2D("weatherTex", clouds.weathermap, 2);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, clouds.weathermap);
	// isn't used
	//cloudsShader.setSampler2D("depthMap", s->sceneFBO->depthTex, 3);
	//glActiveTexture(GL_TEXTURE7);
	//glBindTexture(GL_TEXTURE_2D, sceneFBO->depthTex);

	//cloudsShader.setSampler2D("sky", clouds->sky->getSkyTexture(), 4);
	// TODO
	glActiveTexture(GL_TEXTURE8);
	//glBindTexture(GL_TEXTURE_2D, );

	//actual draw
	//volumetricCloudsShader->draw();
//if(!s->wireframe)
	glDispatchCompute(INT_CEIL(SCR_WIDTH, 16), INT_CEIL(SCR_HEIGHT, 16), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	/*
	if (clouds.postProcess) {
		// cloud post processing filtering
		cloudsPostProcessingFBO->bind();
		Shader &cloudsPPShader = clouds.postProcessingShader->getShader();

		cloudsPPShader.use();

		cloudsPPShader.setSampler2D("clouds", cloudsFBO->getColorAttachmentTex(VolumetricClouds::fragColor), 0);
		cloudsPPShader.setSampler2D("emissions", cloudsFBO->getColorAttachmentTex(VolumetricClouds::bloom), 1);
		cloudsPPShader.setSampler2D("depthMap", s->sceneFBO->depthTex, 2);

		cloudsPPShader.setVec2("cloudRenderResolution", glm::vec2(SCR_WIDTH, SCR_HEIGHT));
		cloudsPPShader.setVec2("resolution", glm::vec2(Window::SCR_WIDTH , Window::SCR_HEIGHT));

		glm::mat4 lightModel;
		lightModel = glm::translate(lightModel, s->lightPos);
		glm::vec4 pos = vp * lightModel * glm::vec4(0.0, 60.0, 0.0, 1.0);
		pos = pos / pos.w;
		pos = pos * 0.5f + 0.5f;

		//std::cout << pos.x << ": X; " << pos.y << " Y;" << std::endl;
		cloudsPPShader.setVec4("lightPos", pos);

		bool isLightInFront = false;
		float lightDotCameraFront = glm::dot(glm::normalize(s->lightPos - s->cam->Position), glm::normalize(s->cam->Front));
		//std::cout << "light dot camera front= " << lightDotCameraFront << std::endl;
		if (lightDotCameraFront > 0.2) {
			isLightInFront = true;
		}

		cloudsPPShader.setBool("isLightInFront", isLightInFront);
		cloudsPPShader.setBool("enableGodRays", clouds.enableGodRays);
		cloudsPPShader.setFloat("lightDotCameraFront", lightDotCameraFront);

		cloudsPPShader.setFloat("time", glfwGetTime());
		if (!s->wireframe)
			clouds.postProcessingShader->draw();
	}
	*/
}

