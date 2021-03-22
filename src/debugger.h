
class Debugger {
public:
	bool exit_request = false;
public:
	void init(SDL_Window *win, SDL_GLContext glcontext);
	void update(int msperframe, glm::vec3 campos);
	void render_navmeshes(void);
	void render_GUI(void);
	// to visualize the navigation meshes
	void add_navmesh(const dtNavMesh *mesh);
	void delete_navmeshes(void);
	void teardown(void);
private:
	SDL_Window *window;
	std::vector<Mesh*> navmeshes;
};

