
class Atlas {
public:
	const glm::vec3 scale = {4096.F, 200.F, 4096.F};
	FloatImage *topology;
	Image *temperature;
	Image *rain;
	//std::vector<struct tile> tiles;
	//std::vector<struct corner> corners;
	//std::vector<struct border> borders;
	//std::list<struct holding> holdings;
public:
	Atlas(uint16_t heightres, uint16_t rainres, uint16_t tempres)
	{
		topology = new FloatImage { heightres, heightres, COLORSPACE_GRAYSCALE };
		rain = new Image { rainres, rainres, COLORSPACE_GRAYSCALE };
		temperature = new Image { tempres, tempres, COLORSPACE_GRAYSCALE };
	}
	~Atlas(void)
	{
		delete topology;
		delete temperature;
		delete rain;
	}
	void generate(int64_t seed);

private:
	//Terragen terragen;
	//Worldgen worldgen;
	//Mosaicfield mosaicfield;
};

