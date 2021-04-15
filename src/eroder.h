
// hydraulic and thermal erosion for terrain heightmaps
class TerrainEroder {
public:
	TerrainEroder(const FloatImage *image);
	~TerrainEroder(void);
	void prestep(const FloatImage *image);
	void waterstep(void);
	void erode(const FloatImage *image);
private:
	uint16_t width;
	uint16_t height;
	Texture *heightmap;
	// ping pong textures
	GLuint erosionmap_read, erosionmap_write; // R: ground, G: water, B: sediment
	GLuint fluxmap_read, fluxmap_write;
	GLuint velocitymap_read, velocitymap_write; // R: x, G: y
	GLuint normalmap;
	// to copy textures
	Shader copy;
	Shader finalcopy;
	// shaders for passes
	Shader rainfall;
	Shader flux;
	Shader velocity;
	Shader sediment;
	Shader advection;
private:
	void init_textures(void);
	void rain_pass(void);
	void flux_pass(float time);
	void velocity_pass(float time);
	void erosion_disposition_pass(void);
	void advection_pass(float time);
};
