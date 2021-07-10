
enum creature_animation_t {
	CA_IDLE,
	CA_WALK,
	CA_RUN,
	CA_FALLING,
	CA_LEFT_STRAFE,
	CA_RIGHT_STRAFE
};

enum creature_movement_t {
	CM_FORWARD,
	CM_LEFT,
	CM_RIGHT,
	CM_BACKWARD,
	CM_LEFT_FORWARD,
	CM_RIGHT_FORWARD
};

class Creature : public Entity {
public:
	float m_animation_mix = 0.f;
	bool m_ragdoll_mode = false;
	const gfx::Model *m_model;
public:
	Creature(const glm::vec3 &pos, const glm::quat &rot, const gfx::Model *model, const module::ragdoll_armature_import_t &armature);
	btRigidBody* get_body() const;
	void control(const glm::vec3 &view, bool forward, bool backward, bool right, bool left);
	void move(const glm::vec2 &velocity);
	void stick_to_agent(const glm::vec3 &agent_position, const glm::vec3 &agent_velocity);
	void jump();
	void update(const btDynamicsWorld *world);
	void sync(float delta);
	const gfx::TransformBuffer* joints() const { return &m_joint_transforms; };
	void add_ragdoll(btDynamicsWorld *world);
	void remove_ragdoll(btDynamicsWorld *world);
private:
	std::unique_ptr<physics::Bumper> m_bumper;
	std::vector<std::pair<uint32_t, uint32_t>> m_targets;
	physics::Ragdoll m_ragdoll;
	std::unique_ptr<util::Animator> m_animator;
	gfx::TransformBuffer m_joint_transforms;
	glm::vec3 m_velocity;
	glm::vec2 m_direction;
	enum creature_animation_t m_current_animation;
	enum creature_animation_t m_previous_animation;
	enum creature_movement_t m_current_movement;
	float m_speed = 1.f;
private:
	void change_animation(enum creature_animation_t anim);
};
