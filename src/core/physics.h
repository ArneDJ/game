
class PhysicsManager {
public:
	PhysicsManager(void);
	~PhysicsManager(void);
	void update(float timestep);
	const btRigidBody* add_dynamic_body(void);
private:
	btCollisionConfiguration *config;
	btCollisionDispatcher *dispatcher;
	btBroadphaseInterface *broadphase;
	btConstraintSolver *solver;
	btDynamicsWorld *world;
private:
	btAlignedObjectArray<btCollisionShape*> shapes;
};

glm::vec3 body_position(const btRigidBody *body);

glm::quat body_rotation(const btRigidBody *body);
