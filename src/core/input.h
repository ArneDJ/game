
class InputManager {
public:
	InputManager(void);
	void update(void);
	bool exit_request(void) const;
	bool key_down(uint32_t keyID) const; // is key held down
	bool key_pressed(uint32_t keyID) const; // is key pressed in current update
	glm::vec2 abs_mousecoords(void) const;
	glm::vec2 rel_mousecoords(void) const;
private:
	std::unordered_map<uint32_t, bool> keymap;
	std::unordered_map<uint32_t, bool> previouskeys;
	struct {
		glm::vec2 absolute;
		glm::vec2 relative;
	} mousecoords;
	bool mousegrab = false;
	bool exit = false;
private:
	void press_key(uint32_t keyID);
	void release_key(uint32_t keyID);
	bool was_key_down(uint32_t keyID) const;
	void sample_relative_mousecoords(void);
	void sample_event(const SDL_Event *event);
};
