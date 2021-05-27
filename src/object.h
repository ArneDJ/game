
class DynamicObject : public Entity {
public:
	btRigidBody *body = nullptr;
public:
	DynamicObject(const glm::vec3 &pos, const glm::quat &rot, btCollisionShape *shape)
	{
		position = pos;
		rotation = rot;

		transform.setIdentity();

		btScalar mass(1.f);

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool dynamic = (mass != 0.f);

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
		body->setRollingFriction(0.1f);
		body->setFriction(0.5f);

		// link body to this entity
		body->setUserPointer(this);
	}
	~DynamicObject(void)
	{
		delete motionstate;
		delete body;
	}
	void update(void)
	{
		motionstate->getWorldTransform(transform);
		position = bt_to_vec3(transform.getOrigin());
		rotation = bt_to_quat(transform.getRotation());
	}
private:
	btMotionState *motionstate = nullptr;
	btTransform transform;
};

class StationaryObject : public Entity {
public:
	btRigidBody *body = nullptr;
public:
	StationaryObject(const glm::vec3 &pos, const glm::quat &rot, btCollisionShape *shape)
	{
		position = pos;
		rotation = rot;

		transform.setIdentity();

		transform.setOrigin(vec3_to_bt(pos));
		transform.setRotation(quat_to_bt(rot));

		btVector3 inertia(0, 0, 0);
		motionstate = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(0.f, motionstate, shape, inertia);
		body = new btRigidBody(rbInfo);

		// link body to this entity
		body->setUserPointer(this);
	}
	~StationaryObject(void)
	{
		delete motionstate;
		delete body;
	}
private:
	btTransform transform;
	btMotionState *motionstate = nullptr;
};

// TODO should not use rigid bodies 
class SettlementNode : public Entity {
public:
	btGhostObject *ghost_object = nullptr;
public:
	SettlementNode(const glm::vec3 &pos, const glm::quat &rot, btCollisionShape *shape, uint32_t tile)
	{
		tileref = tile;
		position = pos;
		rotation = rot;

		transform.setIdentity();

		transform.setOrigin(vec3_to_bt(pos));
		transform.setRotation(quat_to_bt(rot));

		ghost_object = new btGhostObject();
		ghost_object->setCollisionShape(shape);
		ghost_object->setWorldTransform(transform);
		// link ghost object to this entity
		ghost_object->setUserPointer(this);
	}
	~SettlementNode(void)
	{
		//delete ghost_object;
		//delete motionstate;
		//delete body;
	}
	uint32_t get_tileref(void) const { return tileref; }
private:
	btTransform transform;
	//btMotionState *motionstate = nullptr;
	uint32_t tileref = 0;
};
