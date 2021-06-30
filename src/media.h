
class MediaManager {
public:
	static void change_path(const std::string &path);
	static const gfx::Texture* load_texture(const std::string &relpath);
	static const gfx::Model* load_model(const std::string &relpath);
	static void teardown(void);
private:
	static std::string basepath;
	static std::map<uint64_t, gfx::Texture*> textures;
	static std::map<uint64_t, gfx::Model*> models;
};
