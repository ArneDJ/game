
enum PATHFINDER_STATE : uint8_t {
	PATH_STATE_FINISHED,
	PATH_STATE_MOVING,
	PATH_STATE_NEXT
};

class Pathfinder {
public:
	Pathfinder(const glm::vec2 &start);
	void reset(const std::list<glm::vec2> &pathway);
	void update(float delta, float speed);
	glm::vec2 at() const;
	glm::vec2 to() const;
	glm::vec2 velo() const;
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

enum ARMY_TARGET_TYPE : uint8_t {
	TARGET_NONE,
	TARGET_SETTLEMENT,
	TARGET_ARMY,
	TARGET_LAND,
	TARGET_SEA
};

// moves on the campaign map
// either controlled by the player or the AI
class ArmyNode : public Entity {
public:
	ArmyNode(glm::vec2 start, float speedy);
	void set_path(const std::list<glm::vec2> &nodes);
	void update(float delta);
	void set_y_offset(float offset);
	void teleport(const glm::vec2 &pos);
	void set_movement_mode(enum ARMY_MOVEMENT_MODE mode) { movement_mode = mode; };
	void set_target_type(enum ARMY_TARGET_TYPE target) { target_type = target; };
public:
	enum ARMY_MOVEMENT_MODE get_movement_mode() const { return movement_mode; };
	enum ARMY_TARGET_TYPE get_target_type() const { return target_type; };
private:
	float speed;
	std::unique_ptr<Pathfinder> pathfinder;
	enum ARMY_MOVEMENT_MODE movement_mode;
	enum ARMY_TARGET_TYPE target_type;
};
