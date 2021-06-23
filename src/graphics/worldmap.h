namespace GRAPHICS {

class Worldmap {
public:
	Worldmap(const glm::vec3 &mapscale, const UTIL::Image<float> *heightmap, const UTIL::Image<uint8_t> *watermap, const UTIL::Image<uint8_t> *rainmap, const UTIL::Image<uint8_t> *materialmasks, const UTIL::Image<uint8_t> *factionsmap);
	~Worldmap(void);
	//void load_materials(const std::vector<const Texture*> textures);
	void add_material(const std::string &name, const Texture *texture);
	void reload(const UTIL::Image<float> *heightmap, const UTIL::Image<uint8_t> *watermap, const UTIL::Image<uint8_t> *rainmap, const UTIL::Image<uint8_t> *materialmasks, const UTIL::Image<uint8_t> *factionsmap);
	void reload_temperature(const UTIL::Image<uint8_t> *temperature);
	void reload_factionsmap(const UTIL::Image<uint8_t> *factionsmap);
	void reload_masks(const UTIL::Image<uint8_t> *mask_image);
	void change_atmosphere(const glm::vec3 &fogclr, float fogfctr,  const glm::vec3 &sunposition);
	void change_groundcolors(const glm::vec3 &dry, const glm::vec3 &lush);
	void set_faction_factor(float factor);
	void display_land(const UTIL::Camera *camera) const;
	void display_water(const UTIL::Camera *camera, float time) const;
private:
	Mesh *patches = nullptr;
	UTIL::Image<float> normalmap;
	Texture *topology = nullptr;
	Texture *nautical = nullptr;
	Texture *rain = nullptr;
	std::unique_ptr<Texture> m_temperature;
	Texture *normals = nullptr;
	Texture *masks = nullptr;
	Texture *factions = nullptr;
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
