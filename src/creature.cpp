#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "bullet/btBulletDynamicsCommon.h"

#include "core/entity.h"
#include "core/image.h"
#include "core/physics.h"
#include "creature.h"

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

Creature::Creature(const glm::vec3 &pos, const glm::quat &rot)
{
	position = pos;
	rotation = rot;

	speed = 6.f;
	velocity = { 0.f, 0.f, 0.f };

	transform.setIdentity();

	btScalar mass(1.f);

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool dynamic = (mass != 0.f);

	// The total height is height+2*radius, so the height is just the height between the center of each 'sphere' of the capsule caps
	shape = new btCapsuleShape(0.25f, 1.3f);

	btVector3 inertia(0, 0, 0);
	if (dynamic) {
		shape->calculateLocalInertia(mass, inertia);
	}

	transform.setOrigin(vec3_to_bt(pos));
	transform.setRotation(quat_to_bt(rot));

	// using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
	motionstate = new btDefaultMotionState(transform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionstate, shape, inertia);
	body = new btRigidBody(rbInfo);
	body->setSleepingThresholds(0.f, 0.f);
	body->setAngularFactor(0.f);
	body->setFriction(0.f);
	body->setRestitution(0.f);

	body->setActivationState(DISABLE_DEACTIVATION);

	// link body to this entity
	body->setUserPointer(this);
}

Creature::~Creature(void)
{
	delete motionstate;
	delete body;
	delete shape;
}
	
btRigidBody* Creature::get_body(void) const
{
	return body;
}

void Creature::move(const glm::vec2 &direction)
{
	velocity.x = speed * direction.x;
	velocity.z = speed * direction.y;
}

void Creature::update(const btDynamicsWorld *world)
{
	ClosestNotMe ray_callback(body);

	world->rayTest(body->getWorldTransform().getOrigin(), body->getWorldTransform().getOrigin() - btVector3(0.0f, 1.f, 0.0f), ray_callback);
	if (ray_callback.hasHit()) {
		float fraction = ray_callback.m_closestHitFraction;
		glm::vec3 current_pos = bt_to_vec3(body->getWorldTransform().getOrigin());
		float hit_pos = current_pos.y - fraction * 1.f; 

		body->getWorldTransform().getOrigin().setX(current_pos.x);
		body->getWorldTransform().getOrigin().setY(hit_pos+1.f);
		body->getWorldTransform().getOrigin().setZ(current_pos.z);

		velocity.y = 0.f;
	} else {
		velocity.y = body->getLinearVelocity().getY();
	}

	body->setLinearVelocity(vec3_to_bt(velocity));

	velocity.x = 0.f;
	velocity.z = 0.f;
}

void Creature::sync(void)
{
	motionstate->getWorldTransform(transform);
	position = bt_to_vec3(transform.getOrigin());
	rotation = bt_to_quat(transform.getRotation());
}
