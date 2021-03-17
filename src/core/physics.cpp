#include <iostream>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <bullet/btBulletDynamicsCommon.h>

#include "physics.h"

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
}

PhysicsManager::~PhysicsManager(void)
{
	delete world;
	delete solver;
	delete broadphase;
	delete dispatcher;
	delete config;
}
