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

#include "core/shader.h"
#include "core/image.h"
#include "core/texture.h"
#include "eroder.h"

#define INT_CEIL(n,d) (int)ceil((float)n/d)

TerrainEroder::TerrainEroder(const FloatImage *image)
{
	width = image->width;
	height = image->height;

	heightmap = new Texture { image };

	init_textures();

	copy.compile("shaders/erosion/copy.comp", GL_COMPUTE_SHADER);
	copy.link();

	finalcopy.compile("shaders/erosion/finalcopy.comp", GL_COMPUTE_SHADER);
	finalcopy.link();

	rainfall.compile("shaders/erosion/rainfall.comp", GL_COMPUTE_SHADER);
	rainfall.link();

	flux.compile("shaders/erosion/flux.comp", GL_COMPUTE_SHADER);
	flux.link();

	velocity.compile("shaders/erosion/velocity.comp", GL_COMPUTE_SHADER);
	velocity.link();

	sediment.compile("shaders/erosion/sediment.comp", GL_COMPUTE_SHADER);
	sediment.link();

	advection.compile("shaders/erosion/advection.comp", GL_COMPUTE_SHADER);
	advection.link();
}

TerrainEroder::~TerrainEroder(void)
{
	delete_texture(erosionmap_read);
	delete_texture(erosionmap_write);

	delete_texture(fluxmap_read);
	delete_texture(fluxmap_write);

	delete_texture(velocitymap_read);
	delete_texture(velocitymap_write);

	delete_texture(normalmap);

	delete heightmap;
}
	
void TerrainEroder::init_textures(void)
{
	erosionmap_read = generate_2D_texture(NULL, width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	erosionmap_write = generate_2D_texture(NULL, width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	fluxmap_write = generate_2D_texture(NULL, width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	fluxmap_read = generate_2D_texture(NULL, width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	velocitymap_read = generate_2D_texture(NULL, width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	velocitymap_write = generate_2D_texture(NULL, width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	normalmap = generate_2D_texture(NULL, width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
}

void TerrainEroder::prestep(const FloatImage *image)
{
	heightmap->reload(image);

	// copy the height data to the erosion map
	copy.use();
	heightmap->bindimage(0, GL_READ_ONLY, GL_R32F);
	glBindImageTexture(1, erosionmap_read, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
	
void TerrainEroder::waterstep(void)
{
	rain_pass();
}

void TerrainEroder::erode(const FloatImage *image)
{
	/*
	// first copy image data from CPU to GPU
	heightmap->reload(image);

	// copy the height data to the erosion map
	copy.use();
	heightmap->bindimage(0, GL_READ_ONLY, GL_R32F);
	glBindImageTexture(1, erosionmap_read, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	*/
	
	const float time = 0.01F; // fixed time step
	for (int i = 0; i < 1; i++) {
		// create the water map
		//rain_pass();
		// create the flux map
		flux_pass(time);
		// water surface and velocity field update
		velocity_pass(time);

		erosion_disposition_pass();

		advection_pass(time);
	}

	/*
	// copy the eroded height data to the height map texture
	finalcopy.use();
	glBindImageTexture(0, erosionmap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	heightmap->bindimage(1, GL_WRITE_ONLY, GL_R32F);
	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	
	// finally copy image data from GPU back to CPU
	heightmap->unload(image);
	*/

	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, erosionmap_read);
	glActiveTexture(GL_TEXTURE20);
	glBindTexture(GL_TEXTURE_2D, normalmap);
	glActiveTexture(GL_TEXTURE21);
	glBindTexture(GL_TEXTURE_2D, velocitymap_read);
}

void TerrainEroder::rain_pass(void)
{
	rainfall.use();
	glBindImageTexture(0, erosionmap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(1, erosionmap_write, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	
	std::swap(erosionmap_read, erosionmap_write);
}

void TerrainEroder::flux_pass(float time)
{
	flux.use();
	flux.uniform_float("TIME", time);
	flux.uniform_int("WIDTH", width);
	flux.uniform_int("HEIGHT", height);
	glBindImageTexture(0, erosionmap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(1, fluxmap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(2, fluxmap_write, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	std::swap(fluxmap_read, fluxmap_write);
}

void TerrainEroder::velocity_pass(float time)
{
	velocity.use();
	velocity.uniform_float("TIME", time);
	velocity.uniform_int("WIDTH", width);
	velocity.uniform_int("HEIGHT", height);
	glBindImageTexture(0, erosionmap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(1, fluxmap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(2, erosionmap_write, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindImageTexture(3, velocitymap_write, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	std::swap(erosionmap_read, erosionmap_write);
	std::swap(velocitymap_read, velocitymap_write);
}
	
void TerrainEroder::erosion_disposition_pass(void)
{
	sediment.use();

	glBindImageTexture(0, erosionmap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(1, velocitymap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(2, erosionmap_write, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindImageTexture(3, velocitymap_write, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindImageTexture(4, normalmap, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	std::swap(erosionmap_read, erosionmap_write);
	std::swap(velocitymap_read, velocitymap_write);
}
	
void TerrainEroder::advection_pass(float time)
{
	advection.use();
	advection.uniform_float("TIME", time);

	glBindImageTexture(0, erosionmap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(1, velocitymap_read, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(2, erosionmap_write, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	glDispatchCompute(INT_CEIL(width, 32), INT_CEIL(height, 32), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	
	std::swap(erosionmap_read, erosionmap_write);
}
