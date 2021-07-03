namespace geography {

struct building_t {
	const gfx::Model *model;
	glm::vec3 bounds;
	std::vector<geom::transformation_t> transforms;
};

struct building_group_t {
	module::bounds_t<uint8_t> temperature;
	module::bounds_t<uint8_t> precipitation;
	std::vector<building_t> buildings;
};

struct wall_t {
	std::string model;
	std::vector<geom::transformation_t> transforms;
};

struct tree_t {
	std::string trunk;
	std::string leaves;
	std::string billboard;
	std::vector<geom::transformation_t> transforms;
};

class Landscape {
public:
	glm::vec3 SCALE = { 6144.F, 512.F, 6144.F };
	const geom::rectangle_t SITE_BOUNDS = {
		{ 2048.F, 2048.F },
		{ 4096.F, 4096.F }
	};
public:
	Landscape(const module::Module *mod, uint16_t heightres);
public:
	void load_buildings();
public:
	void clear(void);
	void generate(long campaign_seed, uint32_t tileref, int32_t local_seed, float amplitude, uint8_t precipitation, uint8_t temperature, uint8_t tree_density, uint8_t site_radius, bool walled, bool nautical);
public:
	const util::Image<float>* get_heightmap(void) const;
	const util::Image<uint8_t>* get_normalmap(void) const;
	const util::Image<uint8_t>* get_sitemasks(void) const;
	const std::vector<tree_t>& get_trees(void) const;
	const std::vector<building_group_t>& get_houses(void) const;
	const std::vector<wall_t>& get_walls() const { return m_walls; };
	float sample_heightmap(const glm::vec2 &real) const;
private:
	util::Image<float> heightmap;
	util::Image<float> container;
	util::Image<uint8_t> valleymap;
private:
	util::Image<uint8_t> normalmap;
	util::Image<uint8_t> density;
	util::Image<uint8_t> sitemasks;
	Sitegen sitegen;
	const module::Module *m_module;
private:
	std::vector<tree_t> m_trees;
	std::vector<building_group_t> houses;
	std::vector<wall_t> m_walls;
private:
	void gen_heightmap(int32_t local_seed, float amplitude);
	void gen_forest(int32_t seed, uint8_t precipitation, uint8_t temperature, uint8_t tree_density);
	void place_houses(bool walled, uint8_t radius, int32_t seed, uint8_t temperature);
	void place_walls();
	void fill_wall(geom::segment_t S, wall_t *wall, wall_t *wall_left, wall_t *wall_right, wall_t *wall_both, wall_t *ramp);
	void create_sitemasks(uint8_t radius);
	void create_valleymap(void);
};

};
