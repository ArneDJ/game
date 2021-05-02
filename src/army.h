
enum PATHFINDER_STATE : uint8_t {
	PATH_STATE_FINISHED,
	PATH_STATE_MOVING,
	PATH_STATE_NEXT
};

class Pathfinder {
public:
	Pathfinder(const glm::vec2 &start);
	~Pathfinder(void);
	void reset(const std::list<glm::vec2> &pathway);
	void update(float delta, float speed);
	glm::vec2 at(void) const;
	glm::vec2 velo(void) const;
	void teleport(const glm::vec2 &pos);
private:
	std::list<glm::vec2> nodes;
	glm::vec2 location;
	glm::vec2 destination;
	glm::vec2 origin;
	glm::vec2 velocity;
	float radius;
	enum PATHFINDER_STATE state;
};

enum ARMY_MOVEMENT_MODE : uint8_t {
	MOVEMENT_LAND,
	MOVEMENT_SEA,
	MOVEMENT_EMBARK,
	MOVEMENT_DISEMBARK
};

// moves on the campaign map
// either controlled by the player or the AI
class Army : public Entity {
public:
	Army(glm::vec2 start, float speedy);
	~Army(void);
	void set_path(const std::list<glm::vec2> &nodes);
	void update(float delta);
	void set_y_offset(float offset);
	void teleport(const glm::vec2 &pos);
	enum ARMY_MOVEMENT_MODE get_movement_mode(void) const { return movement_mode; };
	void set_movement_mode(enum ARMY_MOVEMENT_MODE mode) { movement_mode = mode; };
private:
	float speed;
	Pathfinder *pathfinder;
	enum ARMY_MOVEMENT_MODE movement_mode;
};
