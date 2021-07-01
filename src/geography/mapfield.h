namespace geography {

struct mosaictriangle {
	uint32_t index; // index of the tile the triangle belongs to
	uint32_t a, b, c;
};

struct mosaicregion {
	geom::rectangle_t area;
	std::vector<uint32_t> triangles;
};

struct mapfield_result {
	bool found = false;
	uint32_t index;
};

class Mapfield {
public:
	void generate(const std::vector<glm::vec2> &vertdata, const std::vector<mosaictriangle> &mosaictriangles, geom::rectangle_t anchors);
	mapfield_result index_in_field(const glm::vec2 &position) const;
private:
	const size_t REGION_RES = 128;
	int regionscale;
	geom::rectangle_t area;
	std::vector<mosaicregion> regions;
	std::vector<mosaictriangle> triangles;
	std::vector<glm::vec2> vertices;
private:
	bool triangle_in_area(glm::vec2 a, glm::vec2 b, glm::vec2 c, const geom::rectangle_t *area);
	void triangle_in_regions(const mosaictriangle *tess, uint32_t id);
};

};
