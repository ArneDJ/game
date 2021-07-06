
class Battle {
public:
	bool naval = false;
	util::Camera camera;
	physics::PhysicsManager physicsman;
	std::unique_ptr<geography::Landscape> landscape;
	// graphics
	std::unique_ptr<gfx::RenderGroup> ordinary;
	std::unique_ptr<gfx::RenderGroup> creatures;
	std::unique_ptr<gfx::Terrain> terrain;
	std::unique_ptr<gfx::Forest> forest;
	gfx::Skybox skybox;
	// entities
	Creature *player;
	std::vector<StationaryObject*> stationaries;
	std::vector<StationaryObject*> tree_stationaries;
	std::vector<Entity> entities;

public:
	void init(const module::Module *mod, const util::Window *window, const shader_group_t *shaders);
	void load_assets(const module::Module *mod);
	void add_creatures(const module::Module *mod);
	void add_buildings();
	void add_walls();
	void add_trees();
	void cleanup();
	void teardown();
};
	
