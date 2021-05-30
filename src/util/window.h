
namespace UTIL {

class Window {
public:
	SDL_Window *window = nullptr;
	SDL_GLContext glcontext;
	uint16_t width = 0;
	uint16_t height = 0;
public:
	bool open(uint16_t w, uint16_t h);
	void close(void);
	void swap(void);
	void set_fullscreen(void);
};

}
