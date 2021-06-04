
class Creature : public Entity {
public:
	Creature(const glm::vec3 &pos, const glm::quat &rot);
	btRigidBody* get_body() const;
	void move(const glm::vec3 &view, bool forward, bool backward, bool right, bool left);
	void jump();
	void update(const btDynamicsWorld *world);
	void sync();
private:
	std::unique_ptr<PHYSICS::Bumper> m_bumper;
	glm::vec3 m_velocity;
	float m_speed = 1.f;
};
