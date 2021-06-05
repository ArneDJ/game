#include <iostream>
#include <memory>
#include <map>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h> 

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "bullet/btBulletDynamicsCommon.h"
#include "bullet/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "bullet/BulletCollision/CollisionDispatch/btGhostObject.h"

#include "extern/ozz/animation/runtime/animation.h"
#include "extern/ozz/animation/runtime/local_to_model_job.h"
#include "extern/ozz/animation/runtime/sampling_job.h"
#include "extern/ozz/animation/runtime/skeleton.h"
#include "extern/ozz/base/maths/soa_transform.h"

#include "util/entity.h"
#include "util/image.h"
#include "util/animation.h"
#include "graphics/texture.h"
#include "graphics/mesh.h"
#include "graphics/model.h"
#include "physics/heightfield.h"
#include "physics/physics.h"
#include "physics/bumper.h"

#include "creature.h"

static inline glm::quat direction_to_quat(glm::vec2 direction)
{
	float angle = atan2(direction.x, direction.y);
	glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.f, 1.f, 0.f));

	return rotation;
}

Creature::Creature(const glm::vec3 &pos, const glm::quat &rot, const GRAPHICS::Model *model)
{
	position = pos;
	rotation = rot;

	m_model = model;

	m_velocity = { 0.f, 0.f, 0.f };
	m_speed = 6.f;

	m_bumper = std::make_unique<PHYSICS::Bumper>(pos, 0.5f, 2.f);

	const std::vector<std::pair<uint32_t, std::string>> animations = {
		std::make_pair(CA_IDLE, "modules/native/media/animations/fox/idle.ozz"),
		std::make_pair(CA_WALK, "modules/native/media/animations/fox/walk.ozz"),
		std::make_pair(CA_RUN, "modules/native/media/animations/fox/run.ozz")
	};
	m_animator = std::make_unique<UTIL::Animator>("modules/native/media/skeletons/fox.ozz", animations);
	m_joint_transforms.matrices.resize(m_animator->models.size());
	m_joint_transforms.alloc(GL_DYNAMIC_DRAW);
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

	if (forward || backward || right || left) {
		current_animation = CA_RUN;
		rotation = direction_to_quat(dir);
	} else {
		current_animation = CA_IDLE;
	}
}

void Creature::jump()
{
	m_bumper->jump();
}

void Creature::update(const btDynamicsWorld *world)
{
	m_bumper->update(world);
}

void Creature::sync(float delta)
{
	position = m_bumper->position();

	m_animator->update(delta, CA_WALK, current_animation, 1.f);
	for (const auto &skin : m_model->skins) {
		for (int i = 0; i < m_animator->models.size(); i++) {
			m_joint_transforms.matrices[i] = UTIL::ozz_to_mat4(m_animator->models[i]) * skin.inversebinds[i];
		}
	}
	m_joint_transforms.update();
}
