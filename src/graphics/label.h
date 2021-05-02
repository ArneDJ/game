
struct label_batch_t {
	GLuint VAO, VBO, EBO;
};

struct label_vertex_t {
	glm::vec3 position;
	glm::vec3 translation;
	glm::vec3 color;
	glm::vec2 uv;
};

struct label_buffer_t {
	std::vector<label_vertex_t> vertices;
	std::vector<uint32_t> indices;
};

class LabelManager {
public:
	LabelManager(const std::string &fontpath, size_t fontsize);
	~LabelManager(void);
	void add(const std::string &text, const glm::vec3 &color, const glm::vec3 &position);
	void clear(void);
	void display(const Camera *camera) const;
	void set_scale(float scalefactor) { scale = scalefactor; }
private:
	texture_atlas_t *atlas;
	texture_font_t *font;
	struct label_buffer_t glyph_buffer;
	struct label_batch_t glyph_batch;
	Shader shader;
	float scale = 1.f;
};
