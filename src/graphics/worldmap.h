namespace gfx {

class Worldmap {
public:
	Worldmap(const glm::vec3 &mapscale, const UTIL::Image<float> *heightmap, const UTIL::Image<uint8_t> *watermap, const UTIL::Image<uint8_t> *rainmap, const UTIL::Image<uint8_t> *materialmasks, const UTIL::Image<uint8_t> *factionsmap);
	//void load_materials(const std::vector<const Texture*> textures);
	void add_material(const std::string &name, const Texture *texture);
	void reload(const UTIL::Image<float> *heightmap, const UTIL::Image<uint8_t> *watermap, const UTIL::Image<uint8_t> *rainmap, const UTIL::Image<uint8_t> *materialmasks, const UTIL::Image<uint8_t> *factionsmap);
	void reload_temperature(const UTIL::Image<uint8_t> *temperature);
	void reload_factionsmap(const UTIL::Image<uint8_t> *factionsmap);
	void reload_masks(const UTIL::Image<uint8_t> *mask_image);
	void change_atmosphere(const glm::vec3 &fogclr, float fogfctr,  const glm::vec3 &sunposition);
	void change_groundcolors(const glm::vec3 &dry, const glm::vec3 &lush, const glm::vec3 &base_rock_min, const glm::vec3 &base_rock_max, const glm::vec3 desert_rock_min, const glm::vec3 desert_rock_max);
	void set_faction_factor(float factor);
	void display_land(const UTIL::Camera *camera) const;
	void display_water(const UTIL::Camera *camera, float time) const;
private:
	std::unique_ptr<Mesh> patches;
	UTIL::Image<float> normalmap;
	// TODO put these textures in struct
	std::unique_ptr<Texture> topology;
	std::unique_ptr<Texture> nautical;
	std::unique_ptr<Texture> rain;
	std::unique_ptr<Texture> m_temperature;
	std::unique_ptr<Texture> normals;
	std::unique_ptr<Texture> masks;
	std::unique_ptr<Texture> factions;
	//std::vector<const Texture*> materials;
	std::vector<texture_binding_t> materials;
	Shader land;
	Shader water;
	glm::vec3 scale;
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
