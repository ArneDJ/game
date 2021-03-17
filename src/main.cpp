#include <iostream>
#include <unordered_map>
#include <chrono>
#include <map>
#include <vector>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <bullet/btBulletDynamicsCommon.h>

#include "extern/inih/INIReader.h"

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "core/logger.h"
#include "core/image.h"
#include "core/entity.h"
#include "core/camera.h"
#include "core/window.h"
#include "core/input.h"
#include "core/timer.h"
#include "core/shader.h"
#include "core/mesh.h"
#include "core/sky.h"
#include "core/texture.h"
#include "core/model.h"
#include "core/render.h"
#include "core/physics.h"
#include "object.h"
//#include "core/sound.h" // TODO replace SDL_Mixer with OpenAL

class Game {
public:
	void run(void);
private:
	bool running;
	bool debugmode;
	WindowManager windowman;
	InputManager inputman;
	RenderManager renderman;
	PhysicsManager physicsman;
	Timer timer;
	Shader object_shader;
	Shader debug_shader;
	Camera camera;
	Skybox *skybox;
	struct {
		uint16_t window_width;
		uint16_t window_height;
		bool fullscreen;
		int FOV;
		float look_sensitivity;
	} settings;
	std::vector<std::pair<DynamicObject, const GLTF::Model*>> dynamics;
private:
	void init(void);
	void teardown(void);
	void update(void);
	void init_settings(void);
};
	
void Game::init_settings(void)
{
	static const std::string INI_SETTINGS_PATH = "settings.ini";
	INIReader reader = { INI_SETTINGS_PATH.c_str() };
	if (reader.ParseError() != 0) { 
		write_log(LogType::ERROR, std::string("Could not load ini file: " + INI_SETTINGS_PATH));
	}
	settings.window_width = reader.GetInteger("", "WINDOW_WIDTH", 1920);
	settings.window_height = reader.GetInteger("", "WINDOW_HEIGHT", 1080);
	settings.fullscreen = reader.GetBoolean("", "FULLSCREEN", false);
	settings.FOV = reader.GetInteger("", "FOV", 90);
	settings.look_sensitivity = float(reader.GetInteger("", "MOUSE_SENSITIVITY", 1));
	debugmode = reader.GetBoolean("", "DEBUG_MODE", false);
}

void Game::init(void)
{
	running = true;

	// load settings
	init_settings();

	if (!windowman.init(settings.window_width, settings.window_height)) {
		exit(EXIT_FAILURE);
	}

	if (settings.fullscreen) {
		windowman.set_fullscreen();
	}
	
	SDL_SetRelativeMouseMode(SDL_FALSE);

	renderman.init();

	// load shaders
	debug_shader.compile("shaders/debug.vert", GL_VERTEX_SHADER);
	debug_shader.compile("shaders/debug.frag", GL_FRAGMENT_SHADER);
	debug_shader.link();

	object_shader.compile("shaders/object.vert", GL_VERTEX_SHADER);
	object_shader.compile("shaders/object.frag", GL_FRAGMENT_SHADER);
	object_shader.link();

	float aspect_ratio = float(settings.window_width) / float(settings.window_height);
	camera.configure(0.1f, 9001.f, aspect_ratio, float(settings.FOV));
	camera.position = { 10.f, 5.f, -10.f };
	camera.lookat(glm::vec3(0.f, 0.f, 0.f));
	camera.project();

	skybox = new Skybox { glm::vec3(0.447f, 0.639f, 0.784f), glm::vec3(0.647f, 0.623f, 0.672f) };

	// TODO move this to debugger
	if (debugmode) {
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO(); (void)io;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer bindings
		ImGui_ImplSDL2_InitForOpenGL(windowman.window, windowman.glcontext);
		ImGui_ImplOpenGL3_Init("#version 430");
	}
}

void Game::teardown(void)
{
	if (debugmode) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
	}

	delete skybox;

	windowman.teardown();
}

void Game::update(void)
{
	inputman.update();
	if (inputman.exit_request()) {
		running = false;
	}
	
	glm::vec2 rel_mousecoords = settings.look_sensitivity * inputman.rel_mousecoords();
	camera.target(rel_mousecoords);

	float modifier = 2.f * timer.delta;
	if (inputman.key_down(SDLK_w)) { camera.move_forward(modifier); }
	if (inputman.key_down(SDLK_s)) { camera.move_backward(modifier); }
	if (inputman.key_down(SDLK_d)) { camera.move_right(modifier); }
	if (inputman.key_down(SDLK_a)) { camera.move_left(modifier); }

	camera.update();

	physicsman.update(timer.delta);
}

void Game::run(void)
{
	init();

	std::vector<glm::vec3> positions;
	for (int i = -10; i < 11; i++) {
		glm::vec3 a = { i, 0.f, -10.f };
		glm::vec3 b = { i, 0.f, 10.f };
		positions.push_back(a);
		positions.push_back(b);
	}
	for (int i = -10; i < 11; i++) {
		glm::vec3 a = { -10, 0.f, i };
		glm::vec3 b = { 10.f, 0.f, i };
		positions.push_back(a);
		positions.push_back(b);
	}
	std::vector<glm::vec2> texcoords;
	
	Mesh grid = { positions, texcoords, GL_LINES };

	GLTF::Model duck = { "media/models/duck.glb", "media/textures/duck.dds" };
	GLTF::Model dragon = { "media/models/dragon.glb", "" };
	GLTF::Model cube = { "media/models/cube.glb", "media/textures/cube.dds" };
	GLTF::Model sphere = { "media/models/sphere.glb", "" };
	GLTF::Model cone = { "media/models/cone.glb", "" };
	GLTF::Model capsule = { "media/models/capsule.glb", "" };
	GLTF::Model cylinder = { "media/models/cylinder.glb", "" };

	physicsman.add_ground_plane(glm::vec3(0.f, 0.f, 0.f));
	DynamicObject cube_ent = {
		physicsman.add_dynamic_body(physicsman.add_box(glm::vec3(1.f, 1.f, 1.f)), glm::vec3(-10.f, 10.f, 10.f))
	};
	DynamicObject sphere_ent = {
		physicsman.add_dynamic_body(physicsman.add_sphere(1.f), glm::vec3(-10.f, 12.f, 10.f))
	};
	DynamicObject cone_ent = {
		physicsman.add_dynamic_body(physicsman.add_cone(1.f, 2.f), glm::vec3(-10.f, 14.f, 10.f))
	};
	DynamicObject capsule_ent = {
		physicsman.add_dynamic_body(physicsman.add_capsule(0.5f, 1.f), glm::vec3(-10.f, 16.f, 10.f))
	};
	DynamicObject cylinder_ent = {
		physicsman.add_dynamic_body(physicsman.add_cylinder(glm::vec3(1.f, 1.f, 1.f)), glm::vec3(-10.f, 18.f, 10.f))
	};
	dynamics.push_back(std::make_pair(cube_ent, &cube));
	dynamics.push_back(std::make_pair(sphere_ent, &sphere));
	dynamics.push_back(std::make_pair(cone_ent, &cone));
	dynamics.push_back(std::make_pair(capsule_ent, &capsule));
	dynamics.push_back(std::make_pair(cylinder_ent, &cylinder));

	while (running) {
		timer.begin();
		update();

		for (auto &dynamic : dynamics) {
			dynamic.first.update();
		}
		
		renderman.prepare_to_render();

		debug_shader.use();
		debug_shader.uniform_vec3("COLOR", glm::vec3(0.f, 1.f, 0.f));
		debug_shader.uniform_mat4("VP", camera.VP);
		debug_shader.uniform_mat4("MODEL", glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f)));
		dragon.display();

		for (const auto &dynamic : dynamics) {
			glm::mat4 T = glm::translate(glm::mat4(1.f), dynamic.first.position);
			glm::mat4 R = glm::mat4(dynamic.first.rotation);
			debug_shader.uniform_mat4("MODEL", T * R);
			dynamic.second->display();
		}

		debug_shader.uniform_mat4("MODEL", glm::mat4(1.f));
		grid.draw();
		object_shader.use();
		object_shader.uniform_mat4("VP", camera.VP);
		object_shader.uniform_bool("INSTANCED", false);

		object_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), glm::vec3(10.f, 0.f, 10.f)));
		duck.display();

		skybox->display(&camera);

		// Start the Dear ImGui frame
		if (debugmode) {
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame(windowman.window);
			ImGui::NewFrame();
			ImGui::Begin("Debug Mode");
			ImGui::SetWindowSize(ImVec2(400, 200));
			if (ImGui::Button("Exit")) { running = false; }
			ImGui::Text("ms per frame: %d", timer.ms_per_frame);
			ImGui::Text("cam position: %f, %f, %f", camera.position.x, camera.position.y, camera.position.z);
			ImGui::End();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		windowman.swap();
		timer.end();
	}
}

int main(int argc, char *argv[])
{
	Game game;
	game.run();

	return 0;
}
