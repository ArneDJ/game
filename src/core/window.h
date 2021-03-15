
class WindowManager {
public:
	SDL_Window *window = nullptr;
	SDL_GLContext glcontext;
	uint16_t width = 0;
	uint16_t height = 0;
public:
	bool init(uint16_t w, uint16_t h);
	void teardown(void);
	void swap(void);
	void set_fullscreen(void);
};
