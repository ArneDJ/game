namespace GRAPHICS {

class Worldmap {
public:
	Worldmap(const glm::vec3 &mapscale, const UTIL::FloatImage *heightmap, const UTIL::Image *watermap, const UTIL::Image *rainmap, const UTIL::Image *materialmasks, const UTIL::Image *factionsmap);
	~Worldmap(void);
	//void load_materials(const std::vector<const Texture*> textures);
	void add_material(const std::string &name, const Texture *texture);
	void reload(const UTIL::FloatImage *heightmap, const UTIL::Image *watermap, const UTIL::Image *rainmap, const UTIL::Image *materialmasks, const UTIL::Image *factionsmap);
	void reload_factionsmap(const UTIL::Image *factionsmap);
	void reload_masks(const UTIL::Image *mask_image);
	void change_atmosphere(const glm::vec3 &fogclr, float fogfctr,  const glm::vec3 &sunposition);
	void change_groundcolors(const glm::vec3 &dry, const glm::vec3 &lush);
	void set_faction_factor(float factor);
	void display_land(const UTIL::Camera *camera) const;
	void display_water(const UTIL::Camera *camera, float time) const;
private:
	Mesh *patches = nullptr;
	UTIL::FloatImage *normalmap = nullptr;
	Texture *topology = nullptr;
	Texture *nautical = nullptr;
	Texture *rain = nullptr;
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
