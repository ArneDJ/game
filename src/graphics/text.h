namespace gfx {

struct glyph_batch_t {
	GLuint VAO, VBO, EBO;
};

struct glyph_vertex_t {
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 uv;
};

struct glyph_buffer_t {
	std::vector<glyph_vertex_t> vertices;
	std::vector<uint32_t> indices;
};

class TextManager {
public:
	TextManager(const std::string &fontpath, size_t fontsize);
	~TextManager(void);
	void add_text(const std::string &text, const glm::vec3 &color, const glm::vec2 &position);
	void clear(void);
	void display(void) const;
private:
	texture_atlas_t *atlas;
	texture_font_t *font;
	glyph_buffer_t glyph_buffer;
	glyph_batch_t glyph_batch;
};

};
