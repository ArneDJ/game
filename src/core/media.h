
class MediaManager {
public:
	void change_path(const std::string &path);
	const Texture* load_texture(const std::string &relpath);
	const GLTF::Model* load_model(const std::string &relpath);
	void teardown(void);
private:
	std::string basepath;
	std::map<uint64_t, Texture*> textures;
	std::map<uint64_t, GLTF::Model*> models;
};
