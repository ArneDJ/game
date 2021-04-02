
struct ray_result {
	bool hit = false;
	glm::vec3 point; // the point the ray intersection happened
};

class PhysicsManager {
public:
	PhysicsManager(void);
	~PhysicsManager(void);
	void update(float timestep);
	void add_ground_plane(const glm::vec3 &position);
	btRigidBody* add_heightfield(const FloatImage *image, const glm::vec3 &scale);
	btCollisionShape* add_box(const glm::vec3 &halfextents);
	btCollisionShape* add_sphere(float radius);
	btCollisionShape* add_cone(float radius, float height);
	btCollisionShape* add_cylinder(const glm::vec3 &halfextents);
	btCollisionShape* add_capsule(float radius, float height);
	btCollisionShape* add_mesh(const std::vector<glm::vec3> &positions, const std::vector<uint16_t> &indices);
	btCollisionShape* add_hull(const std::vector<glm::vec3> &points);
	void insert_body(btRigidBody *body);
	void remove_body(btRigidBody *body);
	void clear(void);
	//
	struct ray_result cast_ray(const glm::vec3 &origin, const glm::vec3 &end);
private:
	btCollisionConfiguration *config;
	btCollisionDispatcher *dispatcher;
	btBroadphaseInterface *broadphase;
	btConstraintSolver *solver;
	btDynamicsWorld *world;
private:
	btAlignedObjectArray<btCollisionShape*> shapes;
	std::vector<btTriangleMesh*> meshes;
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

