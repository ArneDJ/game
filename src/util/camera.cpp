#include <iostream>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.h"

static const glm::vec3 UP_VECTOR = { 0.F, 1.F, 0.F };
static const float MAX_CAMERA_ANGLE = 1.55F;
static const float MIN_CAMERA_ANGLE = -1.55F;
static const float CAMERA_MOUSE_MODIFIER = 0.001F;

namespace util {

void Camera::configure(float near, float far, uint16_t w, uint16_t h, float fovangle)
{
	nearclip = near;
	farclip = far;
	aspectratio = float(w) / float(h);
	width = w;
	height = h;
	FOV = fovangle;
}
	
void Camera::project()
{
	projection = glm::perspective(glm::radians(FOV), aspectratio, nearclip, farclip);
}

void Camera::direct(const glm::vec3 &dir)
{
	direction = glm::normalize(dir);
	// adjust pitch and yaw to direction vector
	pitch = asin(direction.y);
	yaw = atan2(direction.z, direction.x);
}
	
void Camera::angles_to_direction()
{
	direction.x = cos(yaw) * cos(pitch);
	direction.y = sin(pitch);
	direction.z = sin(yaw) * cos(pitch);

	direction = glm::normalize(direction);
}
	
void Camera::lookat(const glm::vec3 &location)
{
	glm::vec3 dir = location - position;
	direct(dir);
}

void Camera::target(const glm::vec2 &offset)
{
	yaw += offset.x * CAMERA_MOUSE_MODIFIER;
	pitch -= offset.y * CAMERA_MOUSE_MODIFIER;

	if (pitch > MAX_CAMERA_ANGLE) { pitch = MAX_CAMERA_ANGLE; }
	if (pitch < MIN_CAMERA_ANGLE) { pitch = MIN_CAMERA_ANGLE; }

	angles_to_direction();
}

void Camera::move_forward(float modifier)
{ 
	position += modifier * direction;
}

void Camera::move_backward(float modifier)
{
	position -= modifier * direction;
}

void Camera::move_left(float modifier)
{
	glm::vec3 left = glm::cross(direction, UP_VECTOR);
	position -= modifier * glm::normalize(left);
}

void Camera::move_right(float modifier)
{
	glm::vec3 right = glm::cross(direction, UP_VECTOR);
	position += modifier * glm::normalize(right);
}

void Camera::update()
{
	viewing = glm::lookAt(position, position + direction, UP_VECTOR);
	VP = projection * viewing;
}

glm::vec3 Camera::ndc_to_ray(const glm::vec2 &ndc) const
{
	glm::vec4 clip = glm::vec4((2.f * ndc.x) / float(width) - 1.f, 1.f - (2.f * ndc.y) / float(height), -1.f, 1.f);

	glm::vec4 worldspace = glm::inverse(projection) * clip;
	glm::vec4 eye = glm::vec4(worldspace.x, worldspace.y, -1.f, 0.f);
	glm::vec3 ray = glm::vec3(glm::inverse(viewing) * eye);

	return glm::normalize(ray);
}
	
void Camera::translate(const glm::vec3 &translation)
{
	position = translation;
}

};
