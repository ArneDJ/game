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

#include "../geometry/geom.h"
#include "../util/image.h"
#include "heightfield.h"
#include "physics.h"

namespace physics {

static const int MAX_SUB_STEPS = 10;
static const btVector3 GRAVITY = { 0.F, -9.81F, 0.F }; // same gravity as in my house

PhysicsManager::PhysicsManager()
{
	m_config = std::make_unique<btDefaultCollisionConfiguration>();
	m_dispatcher = std::make_unique<btCollisionDispatcher>(m_config.get());
	m_broadphase = std::make_unique<btDbvtBroadphase>();
	m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();

	m_world = std::make_unique<btDiscreteDynamicsWorld>(m_dispatcher.get(), m_broadphase.get(), m_solver.get(), m_config.get());
	m_world->setGravity(GRAVITY);
}

PhysicsManager::~PhysicsManager()
{
	clear();
}

void PhysicsManager::clear()
{
	// remove heightfields
	for (auto &field : m_heightfields) {
		m_world->removeCollisionObject(field->object());
	}
	m_heightfields.clear();

	// remove the rigidbodies from the dynamics world and delete them
	for (int i = 0; i < m_world->getNumCollisionObjects(); i++) {
		btCollisionObject *obj = m_world->getCollisionObjectArray()[i];
		btRigidBody *body = btRigidBody::upcast(obj);
		if (body && body->getMotionState()) {
			delete body->getMotionState();
		}
		m_world->removeCollisionObject(obj);
		delete obj;
	}
	// delete collision shapes
	for (int i = 0; i < m_shapes.size(); i++) {
		btCollisionShape *shape = m_shapes[i];
		m_shapes[i] = 0;
		delete shape;
	}

	// delete collision meshes
	for (int i = 0; i < m_meshes.size(); i++) {
		btTriangleMesh *mesh = m_meshes[i];
		m_meshes[i] = 0;
		delete mesh;
	}

	m_meshes.clear();
	m_shapes.clear();
}

void PhysicsManager::update(float timestep)
{
	m_world->stepSimulation(timestep, MAX_SUB_STEPS);
}
	
const btDynamicsWorld* PhysicsManager::get_world() const
{
	return m_world.get();
}

btDynamicsWorld* PhysicsManager::get_world()
{
	return m_world.get();
}

void PhysicsManager::add_shape(btCollisionShape *shape)
{
	m_shapes.push_back(shape);
}

btCollisionShape* PhysicsManager::add_mesh(const std::vector<glm::vec3> &positions, const std::vector<uint16_t> &indices)
{
	if (indices.size() < 3 || positions.size() < 3) {
		LOG(ERROR, "Physics") << "Collision mesh shape error: no triangle data found";
		btCollisionShape *shape = new btSphereShape(5.f);
		add_shape(shape);

		return shape;
	}

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

	m_shapes.push_back(shape);
	m_meshes.push_back(mesh);

	return shape;
}
	
btCollisionShape* PhysicsManager::add_hull(const std::vector<glm::vec3> &points)
{
	if (points.size() < 4) {
		LOG(ERROR, "Physics") << "Collision hull shape error: no point data found";
		btCollisionShape *shape = new btSphereShape(5.f);
		add_shape(shape);

		return shape;
	}

	btConvexHullShape *shape = new btConvexHullShape();

	for (const auto &p : points) {
		shape->addPoint(vec3_to_bt(p));
	}
	shape->optimizeConvexHull();

	return shape;
}
	
void PhysicsManager::add_heightfield(const UTIL::Image<float> *image, const glm::vec3 &scale, int  group, int masks)
{
	auto heightfield = std::make_unique<HeightField>(image, scale);

	add_object(heightfield->object(), group, masks);

	m_heightfields.push_back(std::move(heightfield));
}

void PhysicsManager::add_heightfield(const UTIL::Image<uint8_t> *image, const glm::vec3 &scale, int  group, int masks)
{
	auto heightfield = std::make_unique<HeightField>(image, scale);

	add_object(heightfield->object(), group, masks);

	m_heightfields.push_back(std::move(heightfield));
}

void PhysicsManager::insert_ghost_object(btGhostObject *ghost_object)
{
	m_world->addCollisionObject(ghost_object);
}

void PhysicsManager::add_object(btCollisionObject *object, int groups, int masks)
{
	m_world->addCollisionObject(object, groups, masks);
}
	
void PhysicsManager::add_ghost_object(btGhostObject *object, int groups, int masks)
{
	m_world->addCollisionObject(object, groups, masks);
}
	
void PhysicsManager::add_body(btRigidBody *body, int groups, int masks)
{
	m_world->addRigidBody(body, groups, masks);
}

void PhysicsManager::insert_body(btRigidBody *body)
{
	m_world->addRigidBody(body);
}
	
void PhysicsManager::remove_body(btRigidBody *body)
{
	m_world->removeCollisionObject(body);
}
	
struct ray_result_t PhysicsManager::cast_ray(const glm::vec3 &origin, const glm::vec3 &end, int masks)
{
	struct ray_result_t result = { false, end, nullptr };

	btVector3 from = vec3_to_bt(origin);
	btVector3 to = vec3_to_bt(end);

	btCollisionWorld::ClosestRayResultCallback callback(from, to);
	callback.m_collisionFilterGroup = masks;
	callback.m_collisionFilterGroup |= COLLISION_GROUP_RAY;
	callback.m_collisionFilterMask = masks;
	//callback.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
	//
	m_world->rayTest(from, to, callback);
	if (callback.hasHit()) {
		result.hit = true;
		result.point = bt_to_vec3(callback.m_hitPointWorld);
		result.object = callback.m_collisionObject;
	}

	return result;
}

};

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
