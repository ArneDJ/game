namespace PHYSICS {
 
// left: skeleton target, right: ragdoll transform matrix
// usually a ragdoll joint targets multiple skeleton joints
using ragdoll_target_t = std::pair<uint32_t, uint32_t>;

enum synovial_joint_t : uint8_t {
	JOINT_HINGE,
	JOINT_CONE
};

struct ragdoll_bone_t {
	std::string name;
	btCollisionShape *shape;
	btRigidBody *body;
	/*
	float radius = 1.f; // TODO don't save here
	float height = 1.f; // TODO don't save here
	glm::vec3 origin; // TODO don't save here
	glm::vec3 rotation; // TODO don't save here
	*/
	// multiply the global transform with the inverse bind matrix for vertex skinning
	btTransform origin;
	glm::mat4 transform;
	glm::mat4 inverse;
};

/*
struct constraint_part_t {
	uint32_t bone; // TODO don't save here
	glm::vec3 origin; // TODO don't save here
	glm::vec3 rotation; // TODO don't save here
};
*/

/*
struct ragdoll_joint_t {
	std::unique_ptr<btTypedConstraint> constraint;
	enum synovial_joint_t type; // TODO don't save here
	glm::vec3 limit; // TODO don't save here
	struct constraint_part_t parent; // TODO don't save here
	struct constraint_part_t child; // TODO don't save here
};
*/

class Ragdoll {
public:
	~Ragdoll()
	{
		clean();
	}
	glm::vec3 position;
	glm::quat rotation;
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
	void add_to_world(btDynamicsWorld *world, const glm::vec3 &position);
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
private:
	//std::vector<glm::mat4> m_transforms;
	std::vector<struct ragdoll_bone_t> m_bones;
	std::vector<std::unique_ptr<btTypedConstraint>> m_joints;
};

};
