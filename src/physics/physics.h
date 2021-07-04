
namespace physics {

enum collision_group_t {
    COLLISION_GROUP_NONE = 0,
    COLLISION_GROUP_WORLD = 1 << 0,
    COLLISION_GROUP_GHOSTS = 1 << 1,
    COLLISION_GROUP_ACTOR = 1 << 2,
    COLLISION_GROUP_HEIGHTMAP = 1 << 3,
    COLLISION_GROUP_RAY = 1 << 4,
    COLLISION_GROUP_RAGDOLL = 1 << 5
};

struct ray_result_t {
	bool hit = false;
	glm::vec3 point; // the point where the ray intersection happened
	const btCollisionObject *object = nullptr; // the body if ray hit
};

class PhysicsManager {
public:
	PhysicsManager();
	~PhysicsManager();
public:
	const btDynamicsWorld* get_world() const;
	btDynamicsWorld* get_world();
	void update(float timestep);
	void add_heightfield(const util::Image<float> *image, const glm::vec3 &scale, int group, int masks);
	void add_heightfield(const util::Image<uint8_t> *image, const glm::vec3 &scale, int group, int masks);
	void add_shape(btCollisionShape *shape);
	btCollisionShape* add_mesh(const std::vector<glm::vec3> &positions, const std::vector<uint16_t> &indices);
	btCollisionShape* add_hull(const std::vector<glm::vec3> &points);
	void add_object(btCollisionObject *object, int groups, int masks);
	void add_ghost_object(btGhostObject *object, int groups, int masks);
	void add_body(btRigidBody *body, int groups, int masks);
	void insert_body(btRigidBody *body);
	void remove_body(btRigidBody *body);
	void insert_ghost_object(btGhostObject *ghost_object);
	void remove_ghost_object(btGhostObject *object);
	void clear();
	//
	ray_result_t cast_ray(const glm::vec3 &origin, const glm::vec3 &end, int masks = 0);
private:
	std::unique_ptr<btCollisionConfiguration> m_config;
	std::unique_ptr<btCollisionDispatcher> m_dispatcher;
	std::unique_ptr<btBroadphaseInterface> m_broadphase;
	std::unique_ptr<btConstraintSolver> m_solver;
	std::unique_ptr<btDynamicsWorld> m_world;
private:
	btAlignedObjectArray<btCollisionShape*> m_shapes;
	std::vector<btTriangleMesh*> m_meshes;
	std::vector<std::unique_ptr<HeightField>> m_heightfields;
};

};

glm::vec3 body_position(const btRigidBody *body);

glm::quat body_rotation(const btRigidBody *body);

inline glm::vec3 bt_to_vec3(const btVector3 &v)
{
	return glm::vec3(v.x(), v.y(), v.z());
}

// constructor order for glm quaternion is (w, x, y, z)
inline glm::quat bt_to_quat(const btQuaternion &q)
{
	return glm::quat(q.w(), q.x(), q.y(), q.z());
}

inline btQuaternion quat_to_bt(const glm::quat &q)
{
	return btQuaternion(q.x, q.y, q.z, q.w);
}

inline btVector3 vec3_to_bt(const glm::vec3 &v)
{
	return btVector3(v.x, v.y, v.z);
}

