
class MediaManager {
public:
	static void change_path(const std::string &path);
	static const Texture* load_texture(const std::string &relpath);
	static const GLTF::Model* load_model(const std::string &relpath);
	static void teardown(void);
private:
	static std::string basepath;
	static std::map<uint64_t, Texture*> textures;
	static std::map<uint64_t, GLTF::Model*> models;
};
