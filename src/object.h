
class DynamicObject : public Entity {
public:
	DynamicObject(const btRigidBody *rigidbody)
	{
		body = rigidbody;
		position = body_position(rigidbody);
		rotation = body_rotation(rigidbody);
	}
	void update(void)
	{
		position = body_position(body);
		rotation = body_rotation(body);
	}
private:
	const btRigidBody *body;
};

