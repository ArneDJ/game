#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "bullet/btBulletDynamicsCommon.h"
#include "bullet/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "bullet/BulletCollision/CollisionDispatch/btGhostObject.h"

#include "util/entity.h"
#include "util/image.h"
#include "physics/heightfield.h"
#include "physics/physics.h"
#include "physics/bumper.h"

#include "creature.h"

Creature::Creature(const glm::vec3 &pos, const glm::quat &rot)
{
	position = pos;
	rotation = rot;

	m_velocity = { 0.f, 0.f, 0.f };
	m_speed = 6.f;

	m_bumper = std::make_unique<PHYSICS::Bumper>(pos, 0.5f, 2.f);
}

btRigidBody* Creature::get_body() const
{
	return m_bumper->body();
}

void Creature::move(const glm::vec3 &view, bool forward, bool backward, bool right, bool left)
{
	glm::vec2 direction = {0.f, 0.f};
	glm::vec2 dir = glm::normalize(glm::vec2(view.x, view.z));
	if (forward) {
		direction.x += dir.x;
		direction.y += dir.y;
	}
	if (backward) {
		direction.x -= dir.x;
		direction.y -= dir.y;
	}
	if (right) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x += tmp.x;
		direction.y += tmp.z;
	}
	if (left) {
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x -= tmp.x;
		direction.y -= tmp.z;
	}

	m_velocity.x = m_speed * direction.x;
	m_velocity.z = m_speed * direction.y;

	m_bumper->set_velocity(m_velocity.x, m_velocity.z);
}

void Creature::jump()
{
	m_bumper->jump();
}

void Creature::update(const btDynamicsWorld *world)
{
	m_bumper->update(world);
}

void Creature::sync()
{
	position = m_bumper->position();
}
