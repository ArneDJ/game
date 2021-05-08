
class Entity {
public:
	glm::vec3 position;
	glm::quat rotation;
	float scale = 1.f;
	bool visible;
public:
	Entity(void)
	{
		position = { 0.f, 0.f, 0.f };
		rotation = { 1.f, 0.f, 0.f, 0.f };
		scale = 1.f;
		visible = true;
	}
	explicit Entity(const glm::vec3 &pos, const glm::quat &rot)
	{
		position = pos;
		rotation = rot;
		scale = 1.f;
		visible = true;
	}
};
