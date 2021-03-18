
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
		//btDefaultMotionState *motionstate = new btDefaultMotionState(transform);
		motionstate = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionstate, shape, inertia);
		body = new btRigidBody(rbInfo);
		body->setRollingFriction(0.5f);
		body->setFriction(0.5f);
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

