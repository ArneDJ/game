
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

struct battle_info_t {
	uint32_t tileID = 0; // the tile where the battle is fought
	uint32_t settlementID = 0; // settlement if present
	int32_t local_seed = 0;
	uint8_t precipitation = 0;
	uint8_t temperature = 0;
	uint8_t tree_density = 0;
	uint8_t site_radius = 0; // radius of the town if present
	bool fortified = false; // walls present
	// local terrain data
	float amplitude = 0.f; // local terrain amplitude
	glm::vec3 regolith_color = {};
	glm::vec3 rock_color = {};
	// weather and atmosphere
	float fog_factor = 0.0005f;
	glm::vec3 ambiance_color = { 1.f, 1.f, 1.f };
	glm::vec3 sun_position = { 0.f, 1.f, 0.f };
};

enum class campaign_scroll_status {
	NONE,
	FORWARD,
	BACKWARD
};

class Campaign {
public:
	long seed;
	util::Navigation landnav;
	util::Navigation seanav;
	util::Camera camera;
	physics::PhysicsManager collisionman;
	geography::Atlas atlas;
	// graphics
	std::unique_ptr<gfx::Worldmap> worldmap;
	std::unique_ptr<gfx::LabelManager> labelman;
	std::unique_ptr<gfx::RenderGroup> ordinary;
	std::unique_ptr<gfx::RenderGroup> creatures;
	gfx::Skybox skybox;
	// entities
	Entity marker;
	std::unique_ptr<ArmyNode> player;
	std::vector<std::unique_ptr<SettlementNode>> settlement_nodes;
	std::unordered_map<uint32_t, settlement_t> settlements;
	army_t player_army;
	std::vector<Entity*> entities;
	bool show_factions = false;
	bool factions_visible = false;
	battle_info_t battle_info;
public:
	void init(const util::Window *window, const shader_group_t *shaders);
public:
	void save(const std::string &filepath);
	void load(const std::string &filepath);
public:
	void add_heightfields();
	void load_assets();
	void add_armies();
	void add_trees();
	void spawn_settlements();
	void add_settlements();
	void cleanup();
	void teardown();
public:
	void update_camera(const util::Input *input, const util::Window *window, float sensitivity, float delta);
	void update_labels();
	void update_faction_map();
	void offset_entities();
	void change_player_target(const glm::vec3 &ray);
private:
	//navigation_mesh_record m_navmesh_land;
	//navigation_mesh_record m_navmesh_sea;
	float m_scroll_time = 0.f;
	float m_scroll_speed = 1.f;
	enum campaign_scroll_status m_scroll_status = campaign_scroll_status::NONE;
private:
	void collide_camera();
};
	
