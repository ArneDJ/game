
class LabelManager {
public:
	LabelManager(const std::string &fontpath, size_t fontsize);
	~LabelManager(void);
	void add(const std::string &text, const glm::vec3 &color, const glm::vec3 &position);
	void clear(void);
	void display(const Camera *camera) const;
private:
	texture_atlas_t *atlas;
	texture_font_t *font;
	struct glyph_buffer_t glyph_buffer;
	struct glyph_batch_t glyph_batch;
	Shader shader;
};
