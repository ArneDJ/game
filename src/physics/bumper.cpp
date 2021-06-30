#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../bullet/btBulletDynamicsCommon.h"
#include "../bullet/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "../bullet/BulletCollision/CollisionDispatch/btGhostObject.h"

#include "../geometry/geom.h"
#include "../util/image.h"
#include "heightfield.h"
#include "physics.h"

#include "bumper.h"

namespace physics {

class ClosestNotMe : public btCollisionWorld::ClosestRayResultCallback {
public:
	ClosestNotMe(btRigidBody *body) : btCollisionWorld::ClosestRayResultCallback(btVector3(0.0, 0.0, 0.0), btVector3(0.0, 0.0, 0.0))
	{
		me = body;
	}
	virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult &rayResult, bool normalInWorldSpace)
	{
		if (rayResult.m_collisionObject == me) { return 1.0; }

		return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
	}
protected:
	btRigidBody *me;
};
	
Bumper::Bumper(const glm::vec3 &origin, float radius, float length)
{
	m_velocity = { 0.f, 0.f, 0.f };
	m_grounded = false;
	m_height = length;
	m_probe_ground = length;
	m_probe_air = 0.55f * length;

	btTransform transform;
	transform.setIdentity();

	btScalar mass(1.f);

	bool dynamic = (mass != 0.f);

	// The total height is height+2*radius, so the height is just the height between the center of each 'sphere' of the capsule caps
	float shape_height = length - (0.25f * length);
	shape_height -= 2.f * radius;
	if (shape_height < 0.f) {
		shape_height = 0.f;
	}
	m_shape = std::make_unique<btCapsuleShape>(radius, shape_height);

	btVector3 inertia(0, 0, 0);
	if (dynamic) {
		m_shape->calculateLocalInertia(mass, inertia);
	}

	m_offset = (0.5f * (shape_height + (2.f * radius))) + (0.25f * length);
	m_body_offset =  m_offset - (0.5f * length);

	glm::vec3 position = origin;
	// add vertical offset
	position.y += m_body_offset;
	transform.setOrigin(vec3_to_bt(origin));

	// using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
	m_motionstate = std::make_unique<btDefaultMotionState>(transform);
	btRigidBody::btRigidBodyConstructionInfo rbinfo(mass, m_motionstate.get(), m_shape.get(), inertia);
	m_body = std::make_unique<btRigidBody>(rbinfo);
	m_body->setSleepingThresholds(0.f, 0.f);
	m_body->setAngularFactor(0.f);
	m_body->setFriction(0.f);
	m_body->setRestitution(0.f);

	m_body->setActivationState(DISABLE_DEACTIVATION);
}

btRigidBody* Bumper::body() const
{
	return m_body.get();
}

void Bumper::update(const btDynamicsWorld *world)
{
	ClosestNotMe ray_callback(m_body.get());
	ray_callback.m_collisionFilterGroup = COLLISION_GROUP_ACTOR;
	ray_callback.m_collisionFilterMask = COLLISION_GROUP_ACTOR | COLLISION_GROUP_HEIGHTMAP | COLLISION_GROUP_WORLD;
	
	// to avoid "snapping" the probe scales if the bumper is on the ground or in the air
	float probe_scale = m_grounded ? m_probe_ground : m_probe_air;

	world->rayTest(m_body->getWorldTransform().getOrigin(), m_body->getWorldTransform().getOrigin() - btVector3(0.0f, probe_scale, 0.0f), ray_callback);
	if (ray_callback.hasHit()) {
		m_grounded = true;
		float fraction = ray_callback.m_closestHitFraction;
		glm::vec3 current_pos = bt_to_vec3(m_body->getWorldTransform().getOrigin());
		float hit_pos = current_pos.y - fraction * probe_scale; 

		glm::vec3 hitnormal = bt_to_vec3(ray_callback.m_hitNormalWorld);

		m_body->getWorldTransform().getOrigin().setX(current_pos.x);
		m_body->getWorldTransform().getOrigin().setY(hit_pos+m_offset);
		m_body->getWorldTransform().getOrigin().setZ(current_pos.z);

		m_velocity.y = 0.f;
		//float slope = hitnormal.y;
	} else {
		m_grounded = false;
		m_velocity.y = m_body->getLinearVelocity().getY();
	}

	m_body->setLinearVelocity(vec3_to_bt(m_velocity));

	m_velocity.x = 0.f;
	m_velocity.z = 0.f;

	if (m_grounded) {
		m_body->setGravity({ 0, 0, 0 });
	} else {
		m_body->setGravity(world->getGravity());
	}
}

glm::vec3 Bumper::position() const
{
	btTransform transform;
	m_motionstate->getWorldTransform(transform);

	glm::vec3 translation = bt_to_vec3(transform.getOrigin());
	// substract vertical offset
	translation.y -= m_body_offset + (0.5f * m_height);

	return translation;
}
	
bool Bumper::grounded() const
{
	return m_grounded;
}

void Bumper::set_velocity(float x, float z)
{
	m_velocity.x = x;
	m_velocity.z = z;
}

void Bumper::jump()
{
	if (m_grounded) {
		m_grounded = false; // valid jump request so we are no longer on ground
		m_body->applyCentralImpulse(btVector3(m_velocity.x, 5.f, m_velocity.z));
		/*
		btVector3 v = m_body->getLinearVelocity();
		v.setY(20.f);
		m_body->setLinearVelocity(v);
		*/

		// Move upwards slightly so velocity isn't immediately canceled when it detects it as on ground next frame
		const float offset = 0.01f;

		float previous_Y = m_body->getWorldTransform().getOrigin().getY();

		m_body->getWorldTransform().getOrigin().setY(previous_Y + offset);
	}
}

};

