#include <iostream>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <bullet/btBulletDynamicsCommon.h>

#include "physics.h"

static const int MAX_SUB_STEPS = 10;
static const glm::vec3 GRAVITY = { 0.F, -9.81F, 0.F }; // same gravity as in my house

PhysicsManager::PhysicsManager(void)
{
	puts("initializing physics");
	config = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(config);
	broadphase = new btDbvtBroadphase();
	solver = new btSequentialImpulseConstraintSolver();

	world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, config);
	world->setGravity(btVector3(GRAVITY.x , GRAVITY.y, GRAVITY.z));

	// add the ground plane at the bottom
	btCollisionShape *shape = new btStaticPlaneShape(btVector3(0.f, 1.f, 0.f), 0.f);
	shapes.push_back(shape);

	btTransform groundTransform;
	groundTransform.setIdentity();
	groundTransform.setOrigin(btVector3(0, 0, 0));

	btScalar mass(0.);

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool isDynamic = (mass != 0.f);

	btVector3 localInertia(0, 0, 0);
	if (isDynamic) {
		shape->calculateLocalInertia(mass, localInertia);
	}

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, shape, localInertia);
	btRigidBody *body = new btRigidBody(rbInfo);

	//add the body to the dynamics world
	world->addRigidBody(body);
}

PhysicsManager::~PhysicsManager(void)
{
	//remove the rigidbodies from the dynamics world and delete them
	for (int i = 0; i < world->getNumCollisionObjects(); i++) {
		btCollisionObject* obj = world->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState()) {
			delete body->getMotionState();
		}
		world->removeCollisionObject(obj);
		delete obj;
	}
	// delete collision shapes
	for (int i = 0; i < shapes.size(); i++) {
		btCollisionShape *shape = shapes[i];
		shapes[i] = 0;
		delete shape;
	}
	
	delete world;
	delete solver;
	delete broadphase;
	delete dispatcher;
	delete config;
}

void PhysicsManager::update(float timestep)
{
	world->stepSimulation(timestep, MAX_SUB_STEPS);
}
	
const btRigidBody* PhysicsManager::add_dynamic_body(void)
{
	btCollisionShape* colShape = new btBoxShape(btVector3(1,1,1));
	//btCollisionShape* colShape = new btSphereShape(btScalar(1.));
	shapes.push_back(colShape);

	/// Create Dynamic Objects
	btTransform startTransform;
	startTransform.setIdentity();

	btScalar mass(1.f);

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool isDynamic = (mass != 0.f);

	btVector3 localInertia(0, 0, 0);
	if (isDynamic)
	colShape->calculateLocalInertia(mass, localInertia);

	startTransform.setOrigin(btVector3(-5, 10, 5));

	//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, colShape, localInertia);
	btRigidBody* body = new btRigidBody(rbInfo);

	world->addRigidBody(body);

	return body;
}
	
glm::vec3 body_position(const btRigidBody *body)
{
	btTransform t;
	body->getMotionState()->getWorldTransform(t);

	return glm::vec3(float(t.getOrigin().getX()), float(t.getOrigin().getY()), float(t.getOrigin().getZ()));
}

glm::quat body_rotation(const btRigidBody *body)
{
	btTransform t;
	body->getMotionState()->getWorldTransform(t);

	return glm::quat(float(t.getRotation().getW()), float(t.getRotation().getX()), float(t.getRotation().getY()), float(t.getRotation().getZ()));
}
