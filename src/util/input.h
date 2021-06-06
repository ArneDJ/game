
namespace UTIL {

class Input {
public:
	Input();
	void update();
	void update_keymap();
	bool exit_request() const;
	bool key_down(uint32_t keyID) const; // is key held down
	bool key_pressed(uint32_t keyID) const; // is key pressed in current update
	glm::vec2 abs_mousecoords() const;
	glm::vec2 rel_mousecoords() const;
	int mousewheel_y() const;
	bool mouse_grabbed() const { return mousegrab; }
private:
	std::unordered_map<uint32_t, bool> keymap;
	std::unordered_map<uint32_t, bool> previouskeys;
	struct {
		glm::vec2 absolute;
		glm::vec2 relative;
	} mousecoords;
	bool mousegrab = false;
	bool exit = false;
	int mousewheel = 0;
private:
	void press_key(uint32_t keyID);
	void release_key(uint32_t keyID);
	bool was_key_down(uint32_t keyID) const;
	void sample_relative_mousecoords();
	void sample_absolute_mousecoords();
	void sample_event(const SDL_Event *event);
};

};
