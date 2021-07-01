namespace gfx {

struct worldmap_textures_t {
	std::unique_ptr<Texture> topology;
	std::unique_ptr<Texture> nautical;
	std::unique_ptr<Texture> rain;
	std::unique_ptr<Texture> temperature;
	std::unique_ptr<Texture> normals;
	std::unique_ptr<Texture> masks;
	std::unique_ptr<Texture> factions;
};

class Worldmap {
public:
	Worldmap(const glm::vec3 &mapscale, const util::Image<float> *heightmap, const util::Image<uint8_t> *watermap, const util::Image<uint8_t> *rainmap, const util::Image<uint8_t> *materialmasks, const util::Image<uint8_t> *factionsmap);
	void add_material(const std::string &name, const Texture *texture);
	void reload(const util::Image<float> *heightmap, const util::Image<uint8_t> *watermap, const util::Image<uint8_t> *rainmap, const util::Image<uint8_t> *materialmasks, const util::Image<uint8_t> *factionsmap);
	void reload_temperature(const util::Image<uint8_t> *temperature);
	void reload_factionsmap(const util::Image<uint8_t> *factionsmap);
	void reload_masks(const util::Image<uint8_t> *mask_image);
	void change_atmosphere(const glm::vec3 &fogclr, float fogfctr,  const glm::vec3 &sunposition);
	void change_groundcolors(const glm::vec3 &dry, const glm::vec3 &lush, const glm::vec3 &base_rock_min, const glm::vec3 &base_rock_max, const glm::vec3 desert_rock_min, const glm::vec3 desert_rock_max);
	void set_faction_factor(float factor);
	void display_land(const util::Camera *camera) const;
	void display_water(const util::Camera *camera, float time) const;
private:
	std::unique_ptr<Mesh> m_mesh;
	util::Image<float> m_normalmap;
	worldmap_textures_t m_textures;
	std::vector<texture_binding_t> m_materials;
	Shader m_land;
	Shader m_water;
	glm::vec3 m_scale;
	// atmosphere
	float fogfactor;
	glm::vec3 fogcolor;
	glm::vec3 sunpos;
	// ground colors
	glm::vec3 grass_dry;
	glm::vec3 grass_lush;
	float faction_factor;
};

};
