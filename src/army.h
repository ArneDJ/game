
enum PATHFINDER_STATE : uint8_t {
	PATH_STATE_FINISHED,
	PATH_STATE_MOVING,
	PATH_STATE_NEXT
};

class Pathfinder {
public:
	Pathfinder(glm::vec2 start) 
	{
		location = start;
		destination = start;
		origin = start;
		state = PATH_STATE_FINISHED;
		radius = 0.f;
		velocity = glm::vec2(0.f, 0.f);
	}
	~Pathfinder(void) { nodes.clear(); }
	void reset(const std::list<glm::vec2> &pathway)
	{
		if (pathway.size() > 1) {
			nodes.clear();
			nodes = pathway;
			state = PATH_STATE_NEXT;
		} else {
			state = PATH_STATE_FINISHED;
		}
	}
	void update(float delta, float speed)
	{
		if (state == PATH_STATE_NEXT) {
			std::list<glm::vec2>::iterator itr0 = nodes.begin();
			std::list<glm::vec2>::iterator itr1 = std::next(nodes.begin());
			origin = *itr0;
			destination = *itr1;
			velocity = glm::normalize(destination - origin);
			radius = glm::distance(origin, destination);
			state = PATH_STATE_MOVING;
		} else if (state == PATH_STATE_MOVING) {
			glm::vec2 offset = location + delta * speed * velocity;
			if (point_in_circle(offset, origin, radius)) {
				location = offset;
			} else {
				location = destination;
				nodes.pop_front();
				state = PATH_STATE_NEXT;
			}
		}

		if (nodes.size() < 2) {
			state = PATH_STATE_FINISHED;
		}
	}
	glm::vec2 at(void) { return location; }
	glm::vec2 velo(void) { return velocity; }
	void teleport(const glm::vec2 &pos)
	{
		nodes.clear();
		state = PATH_STATE_FINISHED;
		origin = pos;
		location = pos;
	}
private:
	std::list<glm::vec2> nodes;
	glm::vec2 location;
	glm::vec2 destination;
	glm::vec2 origin;
	glm::vec2 velocity;
	float radius;
	enum PATHFINDER_STATE state;
};

// moves on the campaign map
// either controlled by the player or the AI
class Army : public Entity {
public:
	float speed;
public:
	Army(glm::vec2 start, float speedy) : 
		Entity(glm::vec3(start.x, 0.f, start.y), glm::quat(1.f, 0.f, 0.f, 0.f))
	{
		speed = speedy;
		pathfinder = new Pathfinder { start };
	}
	~Army(void) { delete pathfinder; }
	void set_path(const std::list<glm::vec2> &nodes) { pathfinder->reset(nodes); }
	void update(float delta)
	{
		pathfinder->update(delta, speed);

		glm::vec2 location = pathfinder->at();
		position.x = location.x;
		position.z = location.y;

		glm::vec2 dir = glm::normalize(pathfinder->velo());
		float angle = atan2(dir.x, dir.y);
		glm::mat4 R = glm::rotate(glm::mat4(1.f), angle, glm::vec3(0.f, 1.f, 0.f));
		rotation = glm::quat(R);
	}
	void set_y_offset(float offset) { position.y = offset; }
	void reset(const glm::vec2 &pos) 
	{
		// TODO recheck this
		position.x = pos.x;
		position.z = pos.y;
		pathfinder->teleport(pos);
	}
private:
	Pathfinder *pathfinder;
};
