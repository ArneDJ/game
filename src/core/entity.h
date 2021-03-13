
class Entity {
public:
	glm::vec3 position;
	glm::quat rotation;
	bool visible;
public:
	Entity(glm::vec3 pos, glm::quat rot)
	{
		position = pos;
		rotation = rot;
	}
};
