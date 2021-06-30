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

#include "../geometry/geom.h"
#include "../util/image.h"
#include "heightfield.h"

namespace physics {

HeightField::HeightField(const UTIL::Image<float> *image, const glm::vec3 &scale)
{
	m_shape = std::make_unique<btHeightfieldTerrainShape>(image->width(), image->height(), image->raster().data(), 1.f, 0.f, 1.f, 1, PHY_FLOAT, false);

	btVector3 scaling = { 
		scale.x / float(image->width()), 
		scale.y, 
		scale.z / float(image->height())
	};
	m_shape->setLocalScaling(scaling);
	m_shape->setFlipTriangleWinding(true);

	btVector3 origin = { 0.5f * scale.x, 0.5f * scale.y, 0.5f * scale.z };

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(origin);

	m_object = std::make_unique<btCollisionObject>();
	m_object->setCollisionShape(m_shape.get());
	m_object->setWorldTransform(transform);
}

HeightField::HeightField(const UTIL::Image<uint8_t> *image, const glm::vec3 &scale)
{
	float yscale = scale.y / 255.f; // adjust scale

	m_shape = std::make_unique<btHeightfieldTerrainShape>(image->width(), image->height(), image->raster().data(), 1.f, 0.f, 1.f, 1, PHY_UCHAR, false);

	btVector3 scaling = { 
		scale.x / float(image->width()), 
		yscale, 
		scale.z / float(image->height())
	};
	m_shape->setLocalScaling(scaling);
	m_shape->setFlipTriangleWinding(true);

	btVector3 origin = { 0.5f * scale.x, 0.5f * yscale, 0.5f * scale.z };

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(origin);

	m_object = std::make_unique<btCollisionObject>();
	m_object->setCollisionShape(m_shape.get());
	m_object->setWorldTransform(transform);
}

btCollisionObject* HeightField::object()
{
	return m_object.get();
}

const btCollisionObject* HeightField::object() const
{
	return m_object.get();
}

};
