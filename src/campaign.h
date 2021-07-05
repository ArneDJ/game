
/*
struct faction_t {
	std::string name;
	glm::vec3 primary_color;
	glm::vec3 secondary_color;
};
*/

struct settlement_t {
	std::string name;
	uint32_t tileID;
	geom::transformation_t transform;
	// game data
	uint8_t population = 1;
	bool fortified = false;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			name, 
			tileID, 
			transform.position,
			transform.rotation,
			transform.scale,
			population,
			fortified
		);
	}
};

struct army_t {
	std::string name;
	glm::vec2 position;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			name, 
			position
		);
	}
};

class SettlementNode : public Entity {
public:
	std::unique_ptr<btGhostObject> ghost_object;
	std::unique_ptr<btCollisionShape> shape;
public:
	SettlementNode(const glm::vec3 &pos, const glm::quat &rot, uint32_t tile)
	{
		tileref = tile;
		position = pos;
		rotation = rot;

		transform.setIdentity();

		transform.setOrigin(vec3_to_bt(pos));
		transform.setRotation(quat_to_bt(rot));
	
		shape = std::make_unique<btSphereShape>(5.f);

		ghost_object = std::make_unique<btGhostObject>();
		ghost_object->setCollisionShape(shape.get());
		ghost_object->setWorldTransform(transform);
		// link ghost object to this entity
		ghost_object->setUserPointer(this);
	}
	uint32_t get_tileref(void) const { return tileref; }
private:
	btTransform transform;
	uint32_t tileref = 0;
};
