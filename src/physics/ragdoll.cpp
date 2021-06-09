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

#include "../extern/aixlog/aixlog.h"

#include "../util/geom.h"
#include "../util/image.h"
#include "heightfield.h"
#include "physics.h"
#include "../module.h"
#include "ragdoll.h"

static glm::mat4 bullet_to_mat4(const btTransform &t)
{
	const btMatrix3x3 &basis = t.getBasis();

	glm::mat4 m(1.f);

	// rotation
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			m[c][r] = basis[r][c];
		}
	}
	// translation
	btVector3 origin = t.getOrigin();
	m[3][0] = origin.getX();
	m[3][1] = origin.getY();
	m[3][2] = origin.getZ();
	// unit scale
	m[0][3] = 0.0f;
	m[1][3] = 0.0f;
	m[2][3] = 0.0f;
	m[3][3] = 1.0f;

	return m;
}

static btRigidBody* create_rigidbody(btScalar mass, const btTransform &transform, btCollisionShape *shape)
{
	btVector3 intertia(0,0,0);
	if (mass != 0.f) { shape->calculateLocalInertia(mass, intertia); }

	btDefaultMotionState* motion = new btDefaultMotionState(transform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motion, shape, intertia);
	rbInfo.m_additionalDamping = true;
	btRigidBody *body = new btRigidBody(rbInfo);

	// Setup some damping on the bodies
	body->setDamping(0.05f, 0.85f);
	body->setDeactivationTime(0.8f);
	body->setSleepingThresholds(1.6f, 2.5f);
	// prevent terrain clipping
	body->setContactProcessingThreshold(0.25f);
	body->setCcdMotionThreshold(0.0001f);
	body->setCcdSweptSphereRadius(0.05f);

	return body;
}

namespace PHYSICS {
	
void Ragdoll::create(const struct MODULE::ragdoll_armature_import_t &armature)
{
	clean();

	// add bones
	for (const auto &bone_info : armature.bones) {
		struct ragdoll_bone_t bone;
		bone.name = bone_info.name;
		std::cout << bone.name << std::endl;
		bone.shape = new btCapsuleShape(btScalar(bone_info.radius), btScalar(bone_info.height));

		//btTransform transform;
		bone.origin.setIdentity();
		bone.origin.setOrigin(btVector3(btScalar(bone_info.origin.x), btScalar(bone_info.origin.y), btScalar(bone_info.origin.z)));
		bone.origin.getBasis().setEulerZYX(bone_info.rotation.z, bone_info.rotation.y, bone_info.rotation.x);
		bone.inverse = glm::inverse(bullet_to_mat4(bone.origin));
		bone.body = create_rigidbody(btScalar(1.), bone.origin, bone.shape);

		m_bones.push_back(bone);
	}

	// Now setup the constraints
	//btHingeConstraint* hingeC;
	//btConeTwistConstraint* coneC;
	btTransform localA, localB;
	for (const auto &joint : armature.joints) {
		localA.setIdentity();
		localB.setIdentity();
		localA.getBasis().setEulerZYX(joint.parent.rotation.z, joint.parent.rotation.y, joint.parent.rotation.x);
		localA.setOrigin(btVector3(btScalar(joint.parent.origin.x), btScalar(joint.parent.origin.y), btScalar(joint.parent.origin.z)));
		localB.getBasis().setEulerZYX(joint.child.rotation.z, joint.child.rotation.y, joint.child.rotation.x);
		localB.setOrigin(btVector3(btScalar(joint.child.origin.x), btScalar(joint.child.origin.y), btScalar(joint.child.origin.z)));
		if (joint.type == "hinge") { // TODO use enums
			auto hingeC = std::make_unique<btHingeConstraint>(*m_bones[joint.parent.bone].body, *m_bones[joint.child.bone].body, localA, localB);
			hingeC->setLimit(btScalar(joint.limit.x), btScalar(joint.limit.y));
			m_joints.push_back(std::move(hingeC));
			//joint.constraint = hingeC;
			//world->addConstraint(joint.constraint, true);
		} else if (joint.type == "cone") { // TODO use enums
			auto coneC = std::make_unique<btConeTwistConstraint>(*m_bones[joint.parent.bone].body, *m_bones[joint.child.bone].body, localA, localB);
			coneC->setLimit(joint.limit.x, joint.limit.y, joint.limit.z);
			m_joints.push_back(std::move(coneC));
			//joint.constraint = coneC;
			//world->addConstraint(joint.constraint, true);
		}
	}
}
	
void Ragdoll::update()
{
	btRigidBody *root = m_bones[0].body;
	position = body_position(root);
	rotation = body_rotation(root);
	//glm::mat4 ORIGIN = glm::translate(glm::mat4(1.f), position) * glm::mat4((rotation));
	for (auto &bone : m_bones) {
		btRigidBody *body = bone.body;
		glm::mat4 T = glm::translate(glm::mat4(1.f), body_position(body));
		glm::mat4 R = glm::mat4(body_rotation(body));
		glm::mat4 global_joint_transform = T * R;
		bone.transform = global_joint_transform * bone.inverse;
		/*
		for (const auto &target : ragdoll->body_parts[i].animation_targets) {
		matrix_buffer[target] = global_joint_transform * ragdoll->body_parts[i].inverse_bind;
		}
		*/
	}
}

void Ragdoll::add_to_world(btDynamicsWorld *world, const glm::vec3 &position)
{
	btTransform offset;
	offset.setIdentity();
	offset.setOrigin(btVector3(position.x, position.y, position.z));

	for (auto &bone : m_bones) {
		btTransform transform = bone.origin;
		bone.body->setWorldTransform(offset * transform);
		world->addRigidBody(bone.body, COLLISION_GROUP_RAGDOLL, COLLISION_GROUP_HEIGHTMAP | PHYSICS::COLLISION_GROUP_WORLD);
	}

	for (auto &joint : m_joints) {
		world->addConstraint(joint.get(), true);
	}
}

void Ragdoll::remove_from_world(btDynamicsWorld *world)
{
	for (auto &joint : m_joints) {
		world->removeConstraint(joint.get());
	}
	
	for (auto &bone : m_bones) {
		world->removeRigidBody(bone.body);
	}
}

};
