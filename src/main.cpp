#include <iostream>
#include <unordered_map>
#include <chrono>
#include <map>
#include <vector>
#include <span>

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/recast/Recast.h"
#include "extern/recast/DetourNavMesh.h"
#include "extern/recast/DetourNavMeshQuery.h"
#include "extern/recast/ChunkyTriMesh.h"
#include "extern/recast/DetourCrowd.h"

#include "extern/ozz/animation/runtime/animation.h"
#include "extern/ozz/animation/runtime/local_to_model_job.h"
#include "extern/ozz/animation/runtime/sampling_job.h"
#include "extern/ozz/animation/runtime/skeleton.h"
#include "extern/ozz/base/maths/soa_transform.h"

// define this to make bullet and ozz animation compile on Windows
#define BT_NO_SIMD_OPERATOR_OVERLOADS
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
#include "core/animation.h"
#include "core/navigation.h"
#include "crowd.h"
#include "object.h"
#include "debugger.h"
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
	Shader creature_shader;
	Camera camera;
	Skybox *skybox;
	Navigation navigation;
	Crowd *crowd;
	Debugger debugger;
	struct {
		uint16_t window_width;
		uint16_t window_height;
		bool fullscreen;
		int FOV;
		float look_sensitivity;
	} settings;
	std::vector<std::pair<DynamicObject*, const GLTF::Model*>> dynamics;
	bool change_endpoint = false;
	glm::vec3 endpoint = {};
private:
	void init(void);
	void init_settings(void);
	void load_scene(void);
	void clear_scene(void);
	void update(void);
	void teardown(void);
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

	creature_shader.compile("shaders/creature.vert", GL_VERTEX_SHADER);
	creature_shader.compile("shaders/creature.frag", GL_FRAGMENT_SHADER);
	creature_shader.link();

	camera.configure(0.1f, 9001.f, settings.window_width, settings.window_height, float(settings.FOV));
	camera.position = { 10.f, 5.f, -10.f };
	camera.lookat(glm::vec3(0.f, 0.f, 0.f));
	camera.project();

	skybox = new Skybox { glm::vec3(0.447f, 0.639f, 0.784f), glm::vec3(0.647f, 0.623f, 0.672f) };

	if (debugmode) {
		debugger.init(windowman.window, windowman.glcontext);
	}
}

void Game::load_scene(void)
{
	GLTF::Model *sphere = new GLTF::Model { "media/models/sphere.glb", "" };
	GLTF::Model *cone = new GLTF::Model { "media/models/cone.glb", "" };
	GLTF::Model *capsule = new GLTF::Model { "media/models/capsule.glb", "" };
	GLTF::Model *cylinder = new GLTF::Model { "media/models/cylinder.glb", "" };
	GLTF::Model *cube = new GLTF::Model { "media/models/cube.glb", "media/textures/cube.dds" };

	physicsman.add_ground_plane(glm::vec3(0.f, 0.f, 0.f));
	DynamicObject *cube_ent = new DynamicObject {
		glm::vec3(-10.f, 20.f, 10.f), glm::quat(1.f, 0.f, 0.f, 0.f), physicsman.add_box(glm::vec3(1.f, 1.f, 1.f))
	};
	DynamicObject *sphere_ent = new DynamicObject {
		glm::vec3(-10.f, 22.f, 10.f), glm::quat(1.f, 0.f, 0.f, 0.f), physicsman.add_sphere(1.f)
	};
	DynamicObject *cone_ent = new DynamicObject {
		glm::vec3(-10.f, 24.f, 10.f), glm::quat(1.f, 0.f, 0.f, 0.f), physicsman.add_cone(1.f, 2.f)
	};
	DynamicObject *capsule_ent = new DynamicObject {
		glm::vec3(-10.f, 26.f, 10.f), glm::quat(1.f, 0.f, 0.f, 0.f), physicsman.add_capsule(0.5f, 1.f)
	};
	DynamicObject *cylinder_ent = new DynamicObject {
		glm::vec3(-10.f, 30.f, 10.f), glm::quat(1.f, 0.f, 0.f, 0.f), physicsman.add_cylinder(glm::vec3(1.f, 1.f, 1.f))
	};

	physicsman.insert_body(cube_ent->body);
	physicsman.insert_body(sphere_ent->body);
	physicsman.insert_body(cone_ent->body);
	physicsman.insert_body(capsule_ent->body);
	physicsman.insert_body(cylinder_ent->body);

	dynamics.push_back(std::make_pair(cube_ent, cube));
	dynamics.push_back(std::make_pair(sphere_ent, sphere));
	dynamics.push_back(std::make_pair(cone_ent, cone));
	dynamics.push_back(std::make_pair(capsule_ent, capsule));
	dynamics.push_back(std::make_pair(cylinder_ent, cylinder));
}

void Game::clear_scene(void)
{
	// first remove rigid bodies
	for (const auto &dynamic : dynamics) {
		physicsman.remove_body(dynamic.first->body);
		// delete the rigid body
		delete dynamic.first;
		// delete the display model
		delete dynamic.second;
	}

	dynamics.clear();

	// then clear the physics manager
	physicsman.clear();
}

void Game::teardown(void)
{
	delete crowd;

	if (debugmode) {
		debugger.teardown();
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

	for (auto &dynamic : dynamics) {
		dynamic.first->update();
	}

	if (inputman.key_pressed(SDL_BUTTON_RIGHT) == true && inputman.mouse_grabbed() == false) {
		glm::vec3 ray = camera.ndc_to_ray(inputman.abs_mousecoords());
		struct ray_result result = physicsman.cast_ray(camera.position, camera.position + (1000.f * ray));
		if (result.hit) {
			change_endpoint = true;
			endpoint = result.point;
		}
	}

	inputman.update_keymap();
		
	if (change_endpoint) {
		struct polyresult result = navigation.point_on_navmesh(endpoint);
		if (result.found) {
			crowd->retarget_agent(0, result.position, result.poly);
		}
	}

	crowd->update(timer.delta);

	if (debugmode) {
		debugger.update(timer.ms_per_frame, camera.position);
		if (debugger.exit_request) { running = false; }
	}
}

void Game::run(void)
{
	init();
	
	debugger.add_grid(glm::vec2(-20.f, -20.f), glm::vec2(20.f, 20.f));

	GLTF::Model building = { "media/models/building.glb", "" };
	GLTF::Model duck = { "media/models/duck.glb", "media/textures/duck.dds" };
	GLTF::Model dragon = { "media/models/dragon.glb", "" };
	GLTF::Model toroid = { "media/models/toroid.glb", "" };
	GLTF::Model monkey = { "media/models/monkey.glb", "" };
	GLTF::Model cone = { "media/models/cone.glb", "" };
	GLTF::Model human = { "media/models/fox.glb", "" };
	GLTF::Model capsule = { "media/models/capsule.glb", "" };

	btCollisionShape *shape = physicsman.add_box(glm::vec3(1.f, 1.f, 1.f));
	for (const auto &mesh : toroid.collision_trimeshes) {
		shape = physicsman.add_mesh(mesh.positions, mesh.indices);
	}

	StationaryObject stationary = { glm::vec3(-10.f, 5.f, 10.f), glm::quat(1.f, 0.f, 0.f, 0.f), shape };
	for (const auto &mesh : building.collision_trimeshes) {
		shape = physicsman.add_mesh(mesh.positions, mesh.indices);
	}

	glm::vec3 buildingpos = { -20.f, 3.f, -20.f };
	StationaryObject building_ent = { buildingpos, glm::quat(1.f, 0.f, 0.f, 0.f), shape };

	physicsman.insert_body(stationary.body);
	physicsman.insert_body(building_ent.body);

	btCollisionShape *hull = physicsman.add_box(glm::vec3(1.f, 1.f, 1.f));
	for (const auto &hullmesh : monkey.collision_hulls) {
		hull = physicsman.add_hull(hullmesh.points);
	}
	DynamicObject monkey_ent = { glm::vec3(-10.f, 30.f, 10.f), glm::quat(1.f, 0.f, 0.f, 0.f), hull };
	physicsman.insert_body(monkey_ent.body);

	load_scene();
	
	const std::vector<std::string> animationpaths = {
		"media/animations/fox/idle.ozz",
		//"media/animations/fox/walk.ozz",
		"media/animations/fox/run.ozz"
	};
	Animator animator = { "media/skeletons/fox.ozz", animationpaths };
	glm::mat4 creature_T = glm::translate(glm::mat4(1.f), glm::vec3(10.f, 0.f, -10.f));

	TransformBuffer instancebuf;
	instancebuf.matrices.resize(human.skins[0].inversebinds.size());
	instancebuf.alloc(GL_DYNAMIC_DRAW);
	instancebuf.update();

	// create the navigation mesh
	std::vector<float> vertex_soup;
	std::vector<int> index_soup;
	glm::mat4 building_T = glm::translate(glm::mat4(1.f), buildingpos);
	//glm::mat4 R = glm::mat4(construct->rotation);
	//glm::mat4 M = T * R;
	glm::mat4 M = building_T;
	int index_offset = vertex_soup.size()/3;
	for (const auto &mesh : building.collision_trimeshes) {
		for (const auto &pos : mesh.positions) {
			glm::vec4 vertex = {pos.x, pos.y, pos.z, 1.f};
			vertex = M * vertex;
			vertex_soup.push_back(vertex.x);
			vertex_soup.push_back(vertex.y);
			vertex_soup.push_back(vertex.z);
		}
		for (const auto &index : mesh.indices) {
			index_soup.push_back(index_offset + index);
		}
	}
	if (navigation.build(vertex_soup, index_soup)) {
		puts("succesfully built navigation mesh");
	}

	crowd = new Crowd { navigation.navmesh };
	crowd->add_agent(glm::vec3(-14.f, 1.35f, -4.34f), endpoint, navigation.navquery);
	
	if (debugmode) {
		debugger.add_navmesh(navigation.navmesh);
	}

	while (running) {
		timer.begin();

		update();
		monkey_ent.update();

		if (animator.is_valid()) {
			animator.update(timer.delta);
			for (const auto &skin : human.skins) {
				for (int i = 0; i < animator.models.size(); i++) {
					instancebuf.matrices[i] = ozz_to_mat4(animator.models[i]) * skin.inversebinds[i];
				}
			}
			instancebuf.update();
		}

		renderman.prepare_to_render();

		debug_shader.use();
		debug_shader.uniform_vec3("COLOR", glm::vec3(0.f, 1.f, 0.f));
		debug_shader.uniform_mat4("VP", camera.VP);
		debug_shader.uniform_mat4("MODEL", glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f)));
		dragon.display();

		for (const auto &dynamic : dynamics) {
			glm::mat4 T = glm::translate(glm::mat4(1.f), dynamic.first->position);
			glm::mat4 R = glm::mat4(dynamic.first->rotation);
			debug_shader.uniform_mat4("MODEL", T * R);
			dynamic.second->display();
		}
		
		debug_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), stationary.position));
		toroid.display();
		debug_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), monkey_ent.position) * glm::mat4(monkey_ent.rotation));
		monkey.display();

		glm::vec3 agent_pos = crowd->agent_position(0);
		debug_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), agent_pos));
		capsule.display();

		debug_shader.uniform_mat4("MODEL", building_T);
		building.display();

		debug_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), endpoint));
		cone.display();

		if (debugmode) {
			debug_shader.uniform_mat4("MODEL", glm::mat4(1.f));
			debugger.render_grids();
			debugger.render_navmeshes();
		}
	
		object_shader.use();
		object_shader.uniform_mat4("VP", camera.VP);
		object_shader.uniform_bool("INSTANCED", false);

		object_shader.uniform_mat4("MODEL", glm::translate(glm::mat4(1.f), glm::vec3(10.f, 0.f, 10.f)));
		duck.display();

		creature_shader.use();
		creature_shader.uniform_mat4("VP", camera.VP);
		creature_shader.uniform_mat4("MODEL", creature_T);
		instancebuf.bind(GL_TEXTURE10);
		human.display();

		skybox->display(&camera);

		if (debugmode) {
			debugger.render_GUI();
		}

		windowman.swap();
		timer.end();
	}

	delete crowd;
	physicsman.remove_body(stationary.body);
	physicsman.remove_body(building_ent.body);
	physicsman.remove_body(monkey_ent.body);
	clear_scene();
}

int main(int argc, char *argv[])
{
	write_start_log();

	Game game;
	game.run();

	write_exit_log();

	return 0;
}
