
class Creature : public Entity {
public:
	btRigidBody *body = nullptr;
	btCollisionShape *shape = nullptr;
public:
	Creature(const glm::vec3 &pos, const glm::quat &rot);
	~Creature(void);
	void move(const glm::vec2 &direction);
	void update(void);
	void sync(void);
private:
	btMotionState *motionstate = nullptr;
	btTransform transform;
	glm::vec3 velocity;
	float speed;
};
