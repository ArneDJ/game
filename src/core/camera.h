
class Camera {
public:
	glm::vec3 position;
	glm::vec3 direction;
	glm::mat4 projection, viewing;
	glm::mat4 VP; // view * project
	float nearclip = 0.f;
	float farclip = 0.f;
	float aspectratio = 0.f;
	float FOV = 0.f; // in degrees
public:
	Camera(void);
	void configure(float near, float far, float aspect, float fovangle);
	// create perspective projection matrix
	void project(void);
	// properly change the camera direction vector
	void direct(const glm::vec3 &dir);
	// translate mouse coords to direction vector
	void target(const glm::vec2 &offset);
	// direct the camera to look at a location
	void lookat(const glm::vec3 &location);
	// move the camera
	void move_forward(float modifier);
	void move_backward(float modifier);
	void move_left(float modifier);
	void move_right(float modifier);
	// update view matrix
	void update(void);
private:
	float pitch = 0.f;
	float yaw = 0.f;
	// glm::vec4 frustum[6];
};
