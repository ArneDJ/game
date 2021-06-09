namespace PHYSICS {
 
// left: skeleton target, right: ragdoll transform matrix
// usually a ragdoll joint targets multiple skeleton joints
using ragdoll_target_t = std::pair<uint32_t, uint32_t>;

enum synovial_joint_t : uint8_t {
	JOINT_HINGE,
	JOINT_CONE
};

struct ragdoll_bone_t {
	btCollisionShape *shape;
	btRigidBody *body;
	btTransform origin;
	// multiply the global transform with the inverse bind matrix for vertex skinning
	glm::mat4 transform;
	glm::mat4 inverse;
};

class Ragdoll {
public:
	~Ragdoll()
	{
		clean();
	}
public:
	void create(const struct MODULE::ragdoll_armature_import_t &armature);
	void update();
	void clean()
	{
		// TODO replace with unique_ptr
		for (int i = 0; i < m_bones.size(); i++) {
			delete m_bones[i].body;
			delete m_bones[i].shape;
		}
		m_bones.clear();

		m_joints.clear();
	}
	void add_to_world(btDynamicsWorld *world, const glm::vec3 &pos);
	void remove_from_world(btDynamicsWorld *world);
public:
	const std::vector<struct ragdoll_bone_t>& bones() const { return m_bones; };
	const std::vector<std::unique_ptr<btTypedConstraint>>& joints() const { return m_joints; };
	glm::mat4 transform(uint32_t index) const 
	{
		if (index < m_bones.size()) {
			return m_bones[index].transform;
		}

		return glm::mat4(1.f);
	}
	glm::vec3 position() const
	{
		return m_position;
	}
private:
	std::vector<struct ragdoll_bone_t> m_bones;
	std::vector<std::unique_ptr<btTypedConstraint>> m_joints;
	glm::vec3 m_position;
	glm::quat m_rotation;
};

};
