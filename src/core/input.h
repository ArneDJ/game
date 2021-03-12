
class InputManager {
public:
	void update(void);
	void press_key(uint32_t keyID);
	void release_key(uint32_t keyID);
	bool key_down(uint32_t keyID) const; // is key held down
	bool key_pressed(uint32_t keyID) const; // is key pressed in current update
	void set_abs_mousecoords(float x, float y);
	void set_rel_mousecoords(float x, float y);
	glm::vec2 abs_mousecoords(void) const;
	glm::vec2 rel_mousecoords(void) const;
private:
	std::unordered_map<uint32_t, bool> keymap;
	std::unordered_map<uint32_t, bool> previouskeys;
	struct {
		glm::vec2 absolute;
		glm::vec2 relative;
	} mousecoords;
private:
	bool was_key_down(uint32_t keyID) const;
};
