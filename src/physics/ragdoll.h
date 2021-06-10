namespace PHYSICS {
 
// left: skeleton target, right: ragdoll transform matrix
// usually a ragdoll joint targets multiple skeleton joints
using ragdoll_target_t = std::pair<uint32_t, uint32_t>;

enum synovial_joint_t : uint8_t {
	JOINT_HINGE,
	JOINT_CONE
};

class RagdollBone {
public:
	std::unique_ptr<btCollisionShape> shape;
	std::unique_ptr<btRigidBody> body;
	std::unique_ptr<btMotionState> motion;
	btTransform origin;
	// multiply the global transform with the inverse bind matrix for vertex skinning
	glm::mat4 transform;
	glm::mat4 inverse;
public:
	RagdollBone(float radius, float height, const glm::vec3 &start, const glm::vec3 &rotation);
};

class Ragdoll {
public:
	void create(const struct MODULE::ragdoll_armature_import_t &armature);
	void update();
	void clean();
	void add_to_world(btDynamicsWorld *world, const glm::vec3 &pos, const glm::vec3 &velocity);
	void remove_from_world(btDynamicsWorld *world);
public:
	glm::mat4 transform(uint32_t index) const 
	{
		if (index < m_bones.size()) {
			return m_bones[index]->transform;
		}

		return glm::mat4(1.f);
	}
	glm::vec3 position() const
	{
		return m_position;
	}
private:
	std::vector<std::unique_ptr<RagdollBone>> m_bones;
	std::vector<std::unique_ptr<btTypedConstraint>> m_joints;
	glm::vec3 m_position;
	glm::quat m_rotation;
};

};
