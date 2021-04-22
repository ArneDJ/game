
class Entity {
public:
	glm::vec3 position;
	glm::quat rotation;
	bool visible;
public:
	Entity(void)
	{
		position = { 0.f, 0.f, 0.f };
		rotation = { 1.f, 0.f, 0.f, 0.f };
		visible = true;
	}
	Entity(glm::vec3 pos, glm::quat rot)
	{
		position = pos;
		rotation = rot;
		visible = true;
	}
};
