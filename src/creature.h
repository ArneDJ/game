
class Creature : public Entity {
public:
	Creature(const glm::vec3 &pos, const glm::quat &rot);
	~Creature(void);
	btRigidBody* get_body(void) const;
	void move(const glm::vec3 &view, bool forward, bool backward, bool right, bool left);
	void update(const btDynamicsWorld *world);
	void sync(void);
private:
	btRigidBody *body = nullptr;
	btCollisionShape *shape = nullptr;
	btMotionState *motionstate = nullptr;
	btTransform transform;
	glm::vec3 velocity;
	float speed;
};
