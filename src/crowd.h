struct target_result {
	bool found = false;
	glm::vec3 position;
	dtPolyRef ref;
};

class CrowdManager {
public:
	CrowdManager(dtNavMesh *navmesh);
	~CrowdManager();
	void add_agent(glm::vec3 start, glm::vec3 destination, const dtNavMeshQuery *navquery);
	void update(float dt);
	glm::vec3 agent_velocity(uint32_t index);
	glm::vec3 agent_position(uint32_t index);
	struct target_result agent_target(uint32_t index);
	void agent_speed(uint32_t index, float speed);
	void retarget_agent(uint32_t index, glm::vec3 nearest, dtPolyRef poly);
	const dtCrowdAgent* agent_at(uint32_t index);
	dtPolyRef agent_polyref(uint32_t index);
	void teleport_agent(uint32_t index, glm::vec3 position);
	void set_agent_velocity(uint32_t index, glm::vec3 velocity);
private:
	dtCrowd *dtcrowd;
};
