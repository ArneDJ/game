
enum creature_animation_t {
	CA_IDLE,
	CA_WALK,
	CA_RUN,
};

class Creature : public Entity {
public:
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
	enum creature_animation_t current_animation;
	float m_speed = 1.f;
};
