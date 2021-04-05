
// TODO not sure about this
class ResourceManager {
public:
	static const Texture* load_texture(const std::string &fpath);
	void teardown(void) { texturecache.clear(); }
private:
	static TextureCache texturecache;
};
