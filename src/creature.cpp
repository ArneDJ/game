#include <iostream>
#include <memory>
#include <map>
#include <list>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h> 

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/ozz/animation/runtime/animation.h"
#include "extern/ozz/animation/runtime/local_to_model_job.h"
#include "extern/ozz/animation/runtime/sampling_job.h"
#include "extern/ozz/animation/runtime/skeleton.h"
#include "extern/ozz/base/maths/soa_transform.h"

// define this to make bullet and ozz animation compile on Windows
#define BT_NO_SIMD_OPERATOR_OVERLOADS
#include "extern/bullet/btBulletDynamicsCommon.h"
#include "bullet/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "extern/bullet/BulletCollision/CollisionDispatch/btGhostObject.h"

#include "geometry/geom.h"
#include "util/entity.h"
#include "util/image.h"
#include "util/animation.h"
#include "module/module.h"
#include "graphics/texture.h"
#include "graphics/mesh.h"
#include "graphics/model.h"
#include "physics/heightfield.h"
#include "physics/physics.h"
#include "physics/bumper.h"

#include "physics/ragdoll.h"

#include "creature.h"

static inline glm::quat direction_to_quat(glm::vec2 direction)
{
	float angle = atan2(direction.x, direction.y);
	glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.f, 1.f, 0.f));

	return rotation;
}

Creature::Creature(const glm::vec3 &pos, const glm::quat &rot, const gfx::Model *model, const struct MODULE::ragdoll_armature_import_t &armature)
{
	position = pos;
	rotation = rot;
	scale = 0.01f;

	m_model = model;

	m_velocity = { 0.f, 0.f, 0.f };
	m_direction = { 0.f, 0.f };
	m_speed = 6.f;
	
	m_current_animation = CA_FALLING;
	m_previous_animation = CA_IDLE;

	m_current_movement = CM_FORWARD;

	m_bumper = std::make_unique<physics::Bumper>(pos, 0.5f, 2.f);

	const std::vector<std::pair<uint32_t, std::string>> animations = {
		std::make_pair(CA_IDLE, "modules/native/media/animations/human/idle.ozz"),
		std::make_pair(CA_WALK, "modules/native/media/animations/human/walk.ozz"),
		std::make_pair(CA_RUN, "modules/native/media/animations/human/run.ozz"),
		std::make_pair(CA_LEFT_STRAFE, "modules/native/media/animations/human/left_strafe.ozz"),
		std::make_pair(CA_RIGHT_STRAFE, "modules/native/media/animations/human/right_strafe.ozz"),
		std::make_pair(CA_FALLING, "modules/native/media/animations/human/falling.ozz")
	};
	m_animator = std::make_unique<UTIL::Animator>("modules/native/media/skeletons/human.ozz", animations);
	m_joint_transforms.matrices.resize(m_animator->models.size());
	m_joint_transforms.alloc(GL_DYNAMIC_DRAW);

	m_ragdoll.create(armature);
	
	// left: skeleton target, right: ragdoll transform matrix
	std::list<std::pair<uint32_t, std::string>> names;
	for (int i = 0; i < m_animator->skeleton.num_joints(); i++) {
		names.push_back(std::make_pair(i, m_animator->skeleton.joint_names()[i]));
		//std::cout << m_animator->skeleton.joint_names()[i] << std::endl;
	}
	for (int i = 0; i < armature.bones.size(); i++) {
		const auto &bone = armature.bones[i];
		for (auto it = names.begin(); it != names.end(); ){
			auto joint = *it;
			std::string name = joint.second;
			bool found = false;
			for (const auto &pattern : bone.targets) {
				if (name.find(pattern) != std::string::npos) {
					found = true;
					break;
				}
			}
			if (found) {
				m_targets.push_back(std::make_pair(joint.first, i));
				it = names.erase(it);
			} else {
				it++;
			}
		}
	}
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
		m_current_movement = CM_FORWARD;
		direction.x += dir.x;
		direction.y += dir.y;
	}
	if (backward) {
		m_current_movement = CM_BACKWARD;
		direction.x -= dir.x;
		direction.y -= dir.y;
	}
	if (right) {
		m_current_movement = CM_RIGHT;
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x += tmp.x;
		direction.y += tmp.z;
	}
	if (left) {
		m_current_movement = CM_LEFT;
		glm::vec3 tmp(glm::normalize(glm::cross(view, glm::vec3(0.f, 1.f, 0.f))));
		direction.x -= tmp.x;
		direction.y -= tmp.z;
	}

	m_direction = dir;
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

void Creature::sync(float delta)
{
	if (m_ragdoll_mode) {
		scale = 1.f;
		m_ragdoll.update();
		position = m_ragdoll.position();
	} else {
		position = m_bumper->position();
		scale = 0.01f;
	}

	if (!m_bumper->grounded()) {
		change_animation(CA_FALLING);
	} else {
		if (m_velocity.x != 0.f || m_velocity.z != 0.f) {
			if (m_current_movement == CM_LEFT) {
				change_animation(CA_LEFT_STRAFE);
			} else if (m_current_movement == CM_RIGHT) {
				change_animation(CA_RIGHT_STRAFE);
			} else {
				change_animation(CA_RUN);
			}
		} else {
			change_animation(CA_IDLE);
		}
	}

	if (m_animation_mix < 1.f) {
		m_animation_mix += 4.f * delta;
	}
		
	if (m_velocity.x != 0.f || m_velocity.z != 0.f) {
		rotation = direction_to_quat(m_direction);
	}

	m_animator->update(delta, m_previous_animation, m_current_animation, m_animation_mix);

	glm::mat4 ROOT_TRANSFORM = glm::translate(glm::mat4(1.f), position) * glm::mat4(rotation) * glm::scale(glm::mat4(1.f), glm::vec3(scale));
	for (const auto &skin : m_model->skins) {
		if (m_ragdoll_mode) {
			for (int i = 0; i < skin.inversebinds.size(); i++) {
				m_joint_transforms.matrices[i] = glm::inverse(ROOT_TRANSFORM) * m_ragdoll.transform(0);
			}
			for (const auto &target : m_targets) {
				glm::mat4 T = m_ragdoll.transform(target.second); 
				m_joint_transforms.matrices[target.first] = glm::inverse(ROOT_TRANSFORM) * T;
			}
		} else {
			for (int i = 0; i < m_animator->models.size(); i++) {
				m_joint_transforms.matrices[i] = UTIL::ozz_to_mat4(m_animator->models[i]) * skin.inversebinds[i];
			}
		}
	}
	m_joint_transforms.update();
}
	
void Creature::change_animation(enum creature_animation_t anim)
{
	if (m_current_animation != anim) {
		m_previous_animation = m_current_animation;
		m_current_animation = anim;
		m_animation_mix = 0.f;
	}
}

void Creature::add_ragdoll(btDynamicsWorld *world)
{
	m_ragdoll.add_to_world(world, m_bumper->position(), m_velocity);
}

void Creature::remove_ragdoll(btDynamicsWorld *world)
{
	m_ragdoll.remove_from_world(world);
}
