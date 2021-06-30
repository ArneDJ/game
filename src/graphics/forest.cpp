#include <vector>
#include <chrono>
#include <string>
#include <memory>
#include <map>
#include <queue>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../extern/aixlog/aixlog.h"

#include "../geometry/geom.h"
#include "../util/entity.h"
#include "../util/camera.h"
#include "../util/image.h"
#include "shader.h"
#include "texture.h"
#include "mesh.h"
#include "model.h"

#include "forest.h"

namespace gfx {

void BVH::build(const std::vector<tree_instance_t> &instances)
{
	root = new BVH_node_t(instances, BVH_AXIS_Z);

	puts("build linear array of nodes");
	// build linear array of nodes
	std::queue<BVH_node_t*> queue;
	queue.push(root);
	while (!queue.empty()) {
		auto node = queue.front();
		nodes.push_back(node);
		queue.pop();
		if (node->left) {
			queue.push(node->left);
		}
		if (node->right) {
			queue.push(node->right);
		}
		if (node->left == nullptr || node->right == nullptr) {
			leafs.push_back(node);
		}
	}
	std::cout << "total nodes " << nodes.size() << std::endl;
	std::cout << "total leafs " << leafs.size() << std::endl;
}

BVH_node_t::BVH_node_t(const std::vector<tree_instance_t> &instances, enum BVH_axis_t parent_axis)
{
	// calculate total bounding volume
	bounds.min = glm::vec3((std::numeric_limits<float>::max)());
	bounds.max = glm::vec3((std::numeric_limits<float>::min)());
	
	for (const auto &instance : instances) {
		glm::vec3 position = instance.transform.position;
		bounds.min = (glm::min)(bounds.min, instance.model->bounds.min + position);
		bounds.max = (glm::max)(bounds.max, instance.model->bounds.max + position);
	}

	// make leaf
	if (instances.size() < 128) {
		objects = instances;
		return;
	}

	glm::vec3 node_center = 0.5f * (bounds.max + bounds.min);

	//std::cout << "min " << glm::to_string(bounds.min) << std::endl;
	//std::cout << "max " << glm::to_string(bounds.max) << std::endl;

	// create the tree
	auto best_axis = (parent_axis == BVH_AXIS_X) ? BVH_AXIS_Z : BVH_AXIS_X;

	//printf("best axis %d\n", best_axis);
	//puts("build lists to recurse on");
	// build lists to recurse on
	std::vector<tree_instance_t> leftList;
	std::vector<tree_instance_t> rightList;

	for (const auto instance : instances) {
		glm::vec3 min = instance.model->bounds.min + instance.transform.position;
		glm::vec3 max = instance.model->bounds.max + instance.transform.position;
		glm::vec3 center = (max + min) * 0.5f;

		if (center[best_axis] > node_center[best_axis]) {
			rightList.push_back(instance);
		} else {
			leftList.push_back(instance);
		}
	}

	//std::cout << "right list: " << rightList.size() << std::endl;
	//std::cout << "left list: " << leftList.size() << std::endl;

	if (!rightList.empty() && !leftList.empty()) {
		right = new BVH_node_t(rightList, best_axis);
		left = new BVH_node_t(leftList, best_axis);
	} else {
		if (!rightList.empty()) {
			objects = rightList;
		} else {
			objects = leftList;
		}
    	}
	//std::cout << "BVH num nodes: " << objects.size() << std::endl;
}

BVH::~BVH()
{
	clear();
}

void BVH::clear()
{
	for (int i = 0; i < nodes.size(); i++) {
		delete nodes[i];
	}
	nodes.clear();
	leafs.clear();
	root = nullptr;
}

Forest::Forest(const Shader *detail, const Shader *billboard)
{
	m_detailed = detail;
	m_billboard = billboard;
}

void Forest::add_model(const Model *trunk, const Model *leaves, const Model *billboard, const std::vector<const geom::transformation_t*> &transforms)
{
	auto model = std::make_unique<tree_model_t>();
	model->trunk = trunk;
	model->leaves = leaves;
	model->billboard = billboard;
	// calculate bounds of all models except billboard
	model->bounds.min = (glm::min)(trunk->bound_min, leaves->bound_min);
	model->bounds.max = (glm::max)(trunk->bound_max, leaves->bound_max);

	// create transform matrix buffer
	for (const auto &transform : transforms) {
		glm::mat4 T = glm::translate(glm::mat4(1.f), transform->position);
		glm::mat4 R = glm::mat4(transform->rotation);
		glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(transform->scale));
		glm::mat4 M = T * R * S;
		//model->transforms.push_back(M);
		model->detail_transforms.matrices.push_back(M);
		model->billboard_transforms.matrices.push_back(M);
		geom::transformation_t t = {
			transform->position,
			transform->rotation,
			transform->scale
		};
		model->transformations.push_back(t);
	}
	model->detail_transforms.alloc(GL_STATIC_DRAW);
	model->detail_transforms.update();
	model->billboard_transforms.alloc(GL_STATIC_DRAW);
	model->billboard_transforms.update();

	m_models.push_back(std::move(model));
}

void Forest::display(const UTIL::Camera *camera) const
{
	glm::mat4 projection = glm::perspective(glm::radians(camera->FOV), camera->aspectratio, camera->nearclip, 200.f);
	glm::vec4 planes[6];
	geom::frustum_to_planes(camera->projection * camera->viewing, planes);

	for (const auto &model : m_models) {
		model->detail_count = 0;
		model->billboard_count = 0;
	}
	// tree
	// linear
	const geom::sphere_t sphere = { camera->position, 400.f };
	for (const auto leaf : m_bvh.leafs) {
		if (geom::AABB_in_frustum(leaf->bounds.min, leaf->bounds.max, planes)) {
			if (geom::sphere_intersects_AABB(sphere, leaf->bounds)) {
				for (auto &object : leaf->objects) {
					auto model = object.model;
					model->detail_transforms.matrices[model->detail_count++] = object.T;
				}
			} else {
				for (auto &object : leaf->objects) {
					auto model = object.model;
					model->billboard_transforms.matrices[model->billboard_count++] = object.T;
				}
			}
		}
	}

	for (const auto &model : m_models) {
		model->detail_transforms.update();
		model->billboard_transforms.update();
	}

	m_detailed->use();
	m_detailed->uniform_mat4("VP", camera->VP);
	m_detailed->uniform_vec3("CAM_POS", camera->position);
	m_detailed->uniform_bool("INSTANCED", true);

	// draw leaves
	glDisable(GL_CULL_FACE);
	for (const auto &model : m_models) {
		model->detail_transforms.bind(GL_TEXTURE10); // TODO bind to name
		model->leaves->display_instanced(model->detail_count);
	}
	glEnable(GL_CULL_FACE);

	// draw trunks
	for (const auto &model : m_models) {
		model->detail_transforms.bind(GL_TEXTURE10); // TODO bind to name
		model->trunk->display_instanced(model->detail_count);
	}
	
	
	// billboards
	m_billboard->use();
	m_billboard->uniform_mat4("VP", camera->VP);
	m_billboard->uniform_vec3("CAM_POS", camera->position);
	m_billboard->uniform_vec3("CAM_DIR", camera->direction);
	m_billboard->uniform_mat4("PROJECT", camera->projection);
	m_billboard->uniform_mat4("VIEW", camera->viewing);
	m_billboard->uniform_bool("INSTANCED", true);

	for (const auto &model : m_models) {

		model->billboard_transforms.bind(GL_TEXTURE10); // TODO bind to name
		model->billboard->display_instanced(model->billboard_count);
	}
}
	
void Forest::build_hierarchy()
{
	m_bvh.clear();
	
	std::vector<tree_instance_t> instances;
	for (const auto &model : m_models) {
		for (const auto &transform : model->transformations) {
			glm::mat4 T = glm::translate(glm::mat4(1.f), transform.position);
			glm::mat4 R = glm::mat4(transform.rotation);
			glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(transform.scale));
			glm::mat4 M = T * R * S;
			struct tree_instance_t instance = {
				model.get(),
				transform,
				M
			};
			instances.push_back(instance);
		}
	}
	m_bvh.build(instances);
}
	
void Forest::set_atmosphere(const glm::vec3 &sun_position, const glm::vec3 &fog_color, float fog_factor, const glm::vec3 &ambiance)
{
	m_detailed->use();
	m_detailed->uniform_vec3("SUN_POS", sun_position);
	m_detailed->uniform_vec3("FOG_COLOR", fog_color);
	m_detailed->uniform_float("FOG_FACTOR", fog_factor);
	m_detailed->uniform_vec3("AMBIANCE_COLOR", ambiance);

	m_billboard->use();
	m_billboard->uniform_vec3("SUN_POS", sun_position);
	m_billboard->uniform_vec3("FOG_COLOR", fog_color);
	m_billboard->uniform_float("FOG_FACTOR", fog_factor);
	m_billboard->uniform_vec3("AMBIANCE_COLOR", ambiance);
}

void Forest::clear()
{
	m_bvh.clear();
	m_models.clear();
}

};
