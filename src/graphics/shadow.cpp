#include <iostream>
#include <algorithm>
#include <vector>
#include <array>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../util/entity.h"
#include "../util/camera.h"
#include "shadow.h"

namespace gfx {

static const glm::mat4 SCALE_BIAS = {
	0.5F, 0.0F, 0.0F, 0.0F, 
	0.0F, 0.5F, 0.0F, 0.0F, 
	0.0F, 0.0F, 0.5F, 0.0F, 
	0.5F, 0.5F, 0.5F, 1.0F
};

// only a small portion of the visible scene will receive shadows
static const float FARCLIP_ADJUST = 0.03F;

// implementation based on:
// https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmappingcascade/shadowmappingcascade.cpp
static struct depthmap gen_depthmap(GLsizei size, GLsizei layers)
{
	struct depthmap depth;
	depth.height = size;
	depth.width = size;

	// create the depth texture
	glGenTextures(1, &depth.texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, depth.texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, size, size, layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	// create FBO and attach depth texture to it
	glGenFramebuffers(1, &depth.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depth.FBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth.texture, 0);
	glDrawBuffer(GL_NONE);

	GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if(status != GL_FRAMEBUFFER_COMPLETE) {
		std::cout<< "Framebuffer Error: " << std::hex << status << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return depth;
}

Shadow::Shadow(size_t texture_size)
{ 
	depth = gen_depthmap(texture_size, shadowspaces.size());
	biased_shadowspaces.resize(shadowspaces.size());
}
	
Shadow::~Shadow(void)
{
	if (glIsBuffer(depth.FBO) == GL_TRUE) {
		glDeleteBuffers(1, &depth.FBO);
	}
	if (glIsTexture(depth.texture) == GL_TRUE) {
		glDeleteTextures(1, &depth.texture);
	}
}

void Shadow::enable(void)
{
	glCullFace(GL_FRONT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);

	glBindFramebuffer(GL_FRAMEBUFFER, depth.FBO);

	glViewport(0, 0, depth.width, depth.height);
}

void Shadow::disable(void)
{
	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCullFace(GL_BACK);
}

void Shadow::update(const UTIL::Camera *camera, const glm::vec3 &lightpos)
{
	const float near = camera->nearclip;
	const float far = FARCLIP_ADJUST * camera->farclip;
	const float cliprange = far - near;
	const float aspect = camera->aspectratio;

	const float min_z = near;
	const float max_z = near + cliprange;

	const float range = max_z - min_z;
	const float ratio = max_z / min_z;

	const glm::mat4 perspective = glm::perspective(glm::radians(camera->FOV), camera->aspectratio, near, far);

	std::array<float, 4> splits;

	// Calculate split depths based on view camera furstum
	// Based on method presentd in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	const float lambda = 0.85f;
	for (int i = 0; i < shadowspaces.size(); i++) {
		float p = (i + 1) / static_cast<float>(shadowspaces.size());
		float log = min_z * std::pow(ratio, p);
		float uniform = min_z + range * p;
		float d = lambda * (log - uniform) + uniform;
		splits[i] = ((d - near) / cliprange);
	}
	/*
	splits[0] = 0.02;
	splits[1] = 0.04;
	splits[2] = 0.1;
	*/
	splits[3] = 0.5;

	// Calculate orthographic projection matrix for each cascade
	float last_split = 0.f;
	for (int i = 0; i < shadowspaces.size(); i++) {
		float split_dist = splits[i];

		glm::vec3 corners[8] = {
			glm::vec3(-1.0f,  1.0f, -1.0f),
			glm::vec3( 1.0f,  1.0f, -1.0f),
			glm::vec3( 1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3( 1.0f,  1.0f,  1.0f),
			glm::vec3( 1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invcam = glm::inverse(perspective * camera->viewing);
		for (int i = 0; i < 8; i++) {
			glm::vec4 invcorner = invcam * glm::vec4(corners[i], 1.0f);
			corners[i] = invcorner / invcorner.w;
		}

		for (int i = 0; i < 4; i++) {
			glm::vec3 dist = corners[i + 4] - corners[i];
			corners[i + 4] = corners[i] + (dist * split_dist);
			corners[i] = corners[i] + (dist * last_split);
		}

		// Get frustum center
		glm::vec3 center = glm::vec3(0.0f);
		for (int i = 0; i < 8; i++) {
			center += corners[i];
		}
		center /= 8.0f;

		float radius = 0.0f;
		for (int i = 0; i < 8; i++) {
			float distance = glm::length(corners[i] - center);
			radius = std::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 max_extents = glm::vec3(radius);
		glm::vec3 min_extents = -max_extents;

		glm::vec3 lightdir = normalize(-lightpos);
		glm::mat4 lightview = glm::lookAt(center - lightdir * -min_extents.z, center, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightortho = glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, max_extents.z - min_extents.z);

		// Store split distance and matrix in cascade
		splitdepth[i] = (near + split_dist * cliprange);
		shadowspaces[i] = (lightortho * lightview);

		last_split = splits[i];
	}

	// update biased
	for (int i = 0; i < shadowspaces.size(); i++) {
		biased_shadowspaces[i] = SCALE_BIAS * shadowspaces[i];
	}
}

void Shadow::bind_textures(GLenum unit) const 
{
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D_ARRAY, depth.texture);
}

void Shadow::bind_depthmap(uint8_t section)
{
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth.texture, 0, section);
	glClearDepth(1.f);
	glClear(GL_DEPTH_BUFFER_BIT);
}

};
