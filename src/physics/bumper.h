namespace PHYSICS {

class Bumper {
public:
	Bumper(const glm::vec3 &origin, float radius, float length);
public:
	void update(const btDynamicsWorld *world);
	void jump();
	void set_velocity(float x, float z);
public:
	btRigidBody* body() const;
	glm::vec3 position() const;
	bool grounded() const;
private:
	std::unique_ptr<btRigidBody> m_body;
	std::unique_ptr<btCollisionShape> m_shape;
	std::unique_ptr<btMotionState> m_motionstate;
	glm::vec3 m_velocity;
	bool m_grounded = false;
	float m_probe_ground = 0.f;
	float m_probe_air = 0.f;
	float m_height = 1.f;
	float m_offset = 0.f;
	float m_body_offset = 0.f;
};

};
