
class MediaManager {
public:
	static void change_path(const std::string &path);
	static const GRAPHICS::Texture* load_texture(const std::string &relpath);
	static const GRAPHICS::Model* load_model(const std::string &relpath);
	static void teardown(void);
private:
	static std::string basepath;
	static std::map<uint64_t, GRAPHICS::Texture*> textures;
	static std::map<uint64_t, GRAPHICS::Model*> models;
};
