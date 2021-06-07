
enum creature_animation_t {
	CA_IDLE,
	CA_WALK,
	CA_RUN,
	CA_FALLING,
	CA_LEFT_STRAFE,
	CA_RIGHT_STRAFE,
};

enum creature_movement_t {
	CM_FORWARD,
	CM_LEFT,
	CM_RIGHT,
	CM_BACKWARD,
};

class Creature : public Entity {
public:
	float m_animation_mix = 0.f;
	Creature(const glm::vec3 &pos, const glm::quat &rot, const GRAPHICS::Model *model);
	btRigidBody* get_body() const;
	void move(const glm::vec3 &view, bool forward, bool backward, bool right, bool left);
	void jump();
	void update(const btDynamicsWorld *world);
	void sync(float delta);
	const GRAPHICS::TransformBuffer* joints() const { return &m_joint_transforms; };
private:
	const GRAPHICS::Model *m_model;
	std::unique_ptr<PHYSICS::Bumper> m_bumper;
	std::unique_ptr<UTIL::Animator> m_animator;
	GRAPHICS::TransformBuffer m_joint_transforms;
	glm::vec3 m_velocity;
	glm::vec2 m_direction;
	enum creature_animation_t m_current_animation;
	enum creature_animation_t m_previous_animation;
	enum creature_movement_t m_current_movement;
	float m_speed = 1.f;
private:
	void change_animation(enum creature_animation_t anim);
};
