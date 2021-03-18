#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <bullet/btBulletDynamicsCommon.h>

#include "logger.h"
#include "physics.h"

static const int MAX_SUB_STEPS = 10;
static const btVector3 GRAVITY = { 0.F, -9.81F, 0.F }; // same gravity as in my house

PhysicsManager::PhysicsManager(void)
{
	config = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(config);
	broadphase = new btDbvtBroadphase();
	solver = new btSequentialImpulseConstraintSolver();

	world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, config);
	world->setGravity(GRAVITY);
}

PhysicsManager::~PhysicsManager(void)
{
	clear();

	delete world;
	delete solver;
	delete broadphase;
	delete dispatcher;
	delete config;
}

void PhysicsManager::clear(void)
{
	// remove the rigidbodies from the dynamics world and delete them
	for (int i = 0; i < world->getNumCollisionObjects(); i++) {
		btCollisionObject *obj = world->getCollisionObjectArray()[i];
		btRigidBody *body = btRigidBody::upcast(obj);
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

	// delete collision meshes
	for (int i = 0; i < meshes.size(); i++) {
		btTriangleMesh *mesh = meshes[i];
		meshes[i] = 0;
		delete mesh;
	}

	meshes.clear();
	shapes.clear();
}

void PhysicsManager::update(float timestep)
{
	world->stepSimulation(timestep, MAX_SUB_STEPS);
}
	
btCollisionShape* PhysicsManager::add_box(const glm::vec3 &halfextents)
{
	btCollisionShape *shape = new btBoxShape(vec3_to_bt(halfextents));

	shapes.push_back(shape);

	return shape;
}
	
btCollisionShape* PhysicsManager::add_sphere(float radius)
{
	btCollisionShape *shape = new btSphereShape(radius);

	shapes.push_back(shape);

	return shape;
}

btCollisionShape* PhysicsManager::add_cone(float radius, float height)
{
	btCollisionShape *shape = new btConeShape(radius, height);

	shapes.push_back(shape);

	return shape;
}

btCollisionShape* PhysicsManager::add_cylinder(const glm::vec3 &halfextents)
{
	btCollisionShape *shape = new btCylinderShape(vec3_to_bt(halfextents));

	shapes.push_back(shape);

	return shape;
}
	
btCollisionShape* PhysicsManager::add_capsule(float radius, float height)
{
	btCollisionShape *shape = new btCapsuleShape(radius, height);

	shapes.push_back(shape);

	return shape;
}
	
btCollisionShape* PhysicsManager::add_mesh(const std::vector<glm::vec3> &positions, const std::vector<uint16_t> &indices)
{
	if (indices.size() < 3 || positions.size() < 3) {
		write_log(LogType::ERROR, "Collision shape error: no triangle data found");
		return add_box(glm::vec3(1.f, 1.f, 1.f));
	}

	//struct building_shape building;
	btTriangleMesh *mesh = new btTriangleMesh;

	for (int i = 0; i < indices.size(); i += 3) {
		uint16_t index = indices[i];
		btVector3 v0 = vec3_to_bt(positions[index]);
		index = indices[i+1];
		btVector3 v1 = vec3_to_bt(positions[index]);
		index = indices[i+2];
		btVector3 v2 = vec3_to_bt(positions[index]);
		mesh->addTriangle(v0, v1, v2, false);
	}

	btCollisionShape *shape = new btBvhTriangleMeshShape(mesh, false);

	shapes.push_back(shape);
	meshes.push_back(mesh);

	return shape;
}
	
void PhysicsManager::add_ground_plane(const glm::vec3 &position)
{
	btCollisionShape *shape = new btStaticPlaneShape(btVector3(0.f, 1.f, 0.f), 0.f);
	shapes.push_back(shape);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(vec3_to_bt(position));

	btVector3 inertia(0, 0, 0);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(0.f, nullptr, shape, inertia);
	btRigidBody *body = new btRigidBody(rbInfo);

	world->addRigidBody(body);
}
	
void PhysicsManager::insert_body(btRigidBody *body)
{
	world->addRigidBody(body);
}
	
void PhysicsManager::remove_body(btRigidBody *body)
{
	world->removeCollisionObject(body);
}
	
glm::vec3 body_position(const btRigidBody *body)
{
	btTransform t;
	body->getMotionState()->getWorldTransform(t);

	return glm::vec3(float(t.getOrigin().x()), float(t.getOrigin().y()), float(t.getOrigin().z()));
}

glm::quat body_rotation(const btRigidBody *body)
{
	btTransform t;
	body->getMotionState()->getWorldTransform(t);

	return glm::quat(float(t.getRotation().w()), float(t.getRotation().x()), float(t.getRotation().y()), float(t.getRotation().z()));
}
