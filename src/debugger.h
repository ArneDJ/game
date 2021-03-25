
class Debugger {
public:
	void render_navmeshes(void);
	// to visualize the navigation meshes
	void add_navmesh(const dtNavMesh *mesh);
	void delete_navmeshes(void);
	void teardown(void);
private:
	std::vector<Mesh*> navmeshes;
};

