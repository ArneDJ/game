
class PhysicsManager {
public:
	PhysicsManager(void);
	~PhysicsManager(void);
private:
	btCollisionConfiguration *config;
	btCollisionDispatcher *dispatcher;
	btBroadphaseInterface *broadphase;
	btConstraintSolver *solver;
	btDynamicsWorld *world;
};
