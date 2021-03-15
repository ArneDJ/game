#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.h"

static const glm::vec3 UP_VECTOR = {0.F, 1.F, 0.F};
static const float MAX_CAMERA_ANGLE = 1.55F;
static const float MIN_CAMERA_ANGLE = -1.55F;
static const float CAMERA_MOUSE_MODIFIER = 0.001F;

Camera::Camera(void) 
{
	position = { 0.f, 0.f, 0.f };
	direction = { 0.f, 1.f, 0.f };
	projection = {};
	viewing = {};
	VP = {};
	yaw = 0.f;
	pitch = 0.f;
}
	
void Camera::configure(float near, float far, float aspect, float fovangle)
{
	nearclip = near;
	farclip = far;
	aspectratio = aspect;
	FOV = fovangle;
}
	
void Camera::project(void)
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

	direction.x = cos(yaw) * cos(pitch);
	direction.y = sin(pitch);
	direction.z = sin(yaw) * cos(pitch);

	direction = glm::normalize(direction);
}

void Camera::update(void)
{
	viewing = glm::lookAt(position, position + direction, UP_VECTOR);
	VP = projection * viewing;
}

