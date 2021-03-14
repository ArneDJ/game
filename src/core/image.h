
enum COLOR_CHANNEL : uint8_t {
	CHANNEL_RED = 0,
	CHANNEL_GREEN = 1,
	CHANNEL_BLUE = 2,
	CHANNEL_ALPHA = 3
};

enum COLORSPACE_TYPE : uint8_t {
	COLORSPACE_GRAYSCALE = 1,
	COLORSPACE_RGB = 3,
	COLORSPACE_RGBA = 4
};

template <class T>
class Image {
public:
	Image(uint16_t w, uint16_t h, uint8_t chan);
	~Image(void);
	void clear(void);
private:
	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t channels = 0;
	T *data = nullptr;
	size_t size = 0;
};
