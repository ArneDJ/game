#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../extern/aixlog/aixlog.h"

#define CGLTF_IMPLEMENTATION

#include "image.h"
#include "texture.h"
#include "mesh.h"
#include "model.h"

using namespace GLTF;

struct linkage {
	std::map<const cgltf_node*, struct node_t*> nodes;
	std::map<const cgltf_texture*, GLuint> textures;
};

static void print_gltf_error(cgltf_result error);
static void append_buffer(const cgltf_accessor *accessor,  std::vector<uint8_t> &buffer);
static struct node_t load_node(const cgltf_node *gltfnode);
static struct skin_t load_skin(const cgltf_skin *gltfskin);
static glm::mat4 local_node_transform(const struct node_t *n);
static struct collision_trimesh_t load_collision_trimesh(const cgltf_mesh *mesh);
static struct collision_hull_t load_collision_hull(const cgltf_mesh *mesh);

Model::Model(const std::string &filepath)
{
	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));

	cgltf_data *data = nullptr;
	cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);

	if (result == cgltf_result_success) {
		result = cgltf_load_buffers(&options, data, filepath.c_str());
	}

	if (result == cgltf_result_success) {
		result = cgltf_validate(data);
	}

	if (result == cgltf_result_success) {
		load_data(filepath, data);
		find_bounds(data);
	} else {
		LOG(ERROR, "GLTF") << filepath;
		print_gltf_error(result);
	}
		
	cgltf_free(data);
}

Model::~Model(void)
{
	for (int i = 0; i < meshes.size(); i++) {
		delete meshes[i];
	}
}

void Model::load_materials(const std::vector<const Texture*> textures)
{
	materials.clear();
	materials.insert(materials.begin(), textures.begin(), textures.end());
}

void Model::load_data(const std::string &fpath, const cgltf_data *data)
{
	struct linkage link;

	// load node data
	nodes.resize(data->nodes_count);
	for (int i = 0; i < data->nodes_count; i++) {
		nodes[i] = load_node(&data->nodes[i]);
		link.nodes[&data->nodes[i]] = &nodes[i];
	}

	// load mesh data
	for (int i = 0; i < data->meshes_count; i++) {
		const cgltf_mesh *mesh = &data->meshes[i];
		std::string mesh_name = mesh->name ? mesh->name : std::to_string(i);
		if (mesh_name.find("collision_trimesh") != std::string::npos) {
			collision_trimeshes.push_back(load_collision_trimesh(mesh));
		} else if (mesh_name.find("collision_hull") != std::string::npos) {
			collision_hulls.push_back(load_collision_hull(mesh));
		} else {
			load_mesh(mesh);
		}
	}

	// load skin data
	skins.resize(data->skins_count);
	for (int i = 0; i < data->skins_count; i++) {
		skins[i] = load_skin(&data->skins[i]);
	}

	// links
	for (int i = 0; i < data->nodes_count; i++) {
		if (data->nodes[i].parent) {
			nodes[i].parent = link.nodes[data->nodes[i].parent];
		}
		for (int j = 0; j < data->nodes[i].children_count; j++) {
			nodes[i].children.push_back(link.nodes[data->nodes[i].children[j]]);
		}
	}

	for (int i = 0; i < data->skins_count; i++) {
		skins[i].root = link.nodes[data->skins[i].skeleton];
		for (int j = 0; j < data->skins[i].joints_count; j++) {
			struct node_t *n = link.nodes[data->skins[i].joints[j]];
			skins[i].joints.push_back(n);
		}
	}
}
	
void Model::find_bounds(const cgltf_data *data)
{
	bool found = false;

	float min = (std::numeric_limits<float>::max)();
	float max = (std::numeric_limits<float>::min)();
	bound_min = { min, min, min };
	bound_max = { max, max, max };

	for (int i = 0; i < data->meshes_count; i++) {
		const cgltf_mesh *mesh = &data->meshes[i];
		for (int j = 0; j < mesh->primitives_count; j++) {
			const cgltf_primitive *primitive = &mesh->primitives[j];
			for (int k = 0; k < primitive->attributes_count; k++) {
				const cgltf_attribute *attribute = &primitive->attributes[k];
				const cgltf_accessor *accessor = attribute->data;
				if (accessor->has_min && accessor->has_max) {
					found = true;
					glm::vec3 min = { accessor->min[0], accessor->min[1], accessor->min[2] };
					glm::vec3 max = { accessor->max[0], accessor->max[1], accessor->max[2] };
					bound_min = (glm::min)(bound_min, min);
					bound_max = (glm::max)(bound_max, max);
				}
			}
		}
	}
	// default bounding box if not found
	if (!found) {
		bound_min = { -1.f, -1.f, -1.f };
		bound_max = { 1.f, 1.f, 1.f };
	}
}

void Model::display(void) const
{
	for (int i = 0; i < materials.size(); i++) {
		materials[i]->bind(GL_TEXTURE0 + i);
	}

	for (auto mesh : meshes) {
		mesh->draw();
	}
}

void Model::display_instanced(GLsizei count) const
{
	for (int i = 0; i < materials.size(); i++) {
		materials[i]->bind(GL_TEXTURE0 + i);
	}

	for (auto mesh : meshes) {
		mesh->draw_instanced(count);
	}
}

static struct node_t load_node(const cgltf_node *gltfnode)
{
	struct node_t n;
	n.transform = glm::mat4{1.f};
	n.translation = glm::vec3{};
	n.scale = glm::vec3{1.f};
 	n.rotation = glm::quat{};

	n.name = gltfnode->name ? gltfnode->name : "potato";

	if (gltfnode->has_matrix) {
		n.transform = glm::make_mat4(gltfnode->matrix);
	}
	if (gltfnode->has_translation) {
		n.translation = glm::make_vec3(gltfnode->translation);
	}
	if (gltfnode->has_scale) {
		n.scale = glm::make_vec3(gltfnode->scale);
	}
	if (gltfnode->has_rotation) {
		glm::quat q = glm::make_quat(gltfnode->rotation);
		n.rotation = q;
	}

	return n;
}

static inline GLenum primitive_mode(cgltf_primitive_type type)
{
	switch (type) {
	case cgltf_primitive_type_points: return GL_POINTS;
	case cgltf_primitive_type_lines: return GL_LINES;
	case cgltf_primitive_type_line_loop: return GL_LINE_LOOP;
	case cgltf_primitive_type_line_strip: return GL_LINE_STRIP;
	case cgltf_primitive_type_triangles: return GL_TRIANGLES;
	case cgltf_primitive_type_triangle_strip: return GL_TRIANGLE_STRIP;
	case cgltf_primitive_type_triangle_fan: return GL_TRIANGLE_FAN;
	default: return GL_POINTS;
	}
}

void Model::load_mesh(const cgltf_mesh *gltfmesh)
{
	 // index buffer
	std::vector<uint8_t> indices;
	// vertex buffer
	struct vertex_data vertices;

	std::vector<struct primitive> primitives;

	uint32_t vertexstart = 0;
	uint32_t indexstart = 0;
	for (int i = 0; i < gltfmesh->primitives_count; i++) {
		const cgltf_primitive *primitive = &gltfmesh->primitives[i];

		// import index data
		uint32_t indexcount = 0;
		if (primitive->indices) {
			indexcount = primitive->indices->count;
			append_buffer(primitive->indices, indices);
		}

		// An accessor also contains min and max properties that summarize the contents of their data. They are the component-wise minimum and maximum values of all data elements contained in the accessor. In the case of vertex positions, the min and max properties thus define the bounding box of an object.
		// import vertex data
		uint32_t vertexcount = 0;
		for (int j = 0; j < primitive->attributes_count; j++) {
			const cgltf_attribute *attribute = &primitive->attributes[j];
			switch (attribute->type) {
			case cgltf_attribute_type_position:
			vertexcount = attribute->data->count;
			append_buffer(attribute->data, vertices.positions);
			break;
			case cgltf_attribute_type_normal:
			append_buffer(attribute->data, vertices.normals);
			break;
			case cgltf_attribute_type_texcoord:
			append_buffer(attribute->data, vertices.texcoords);
			break;
			case cgltf_attribute_type_joints:
			append_buffer(attribute->data, vertices.joints);
			break;
			case cgltf_attribute_type_weights:
			append_buffer(attribute->data, vertices.weights);
			break;
			}
		}

		struct primitive prim;
		prim.firstindex = indexstart;
		prim.indexcount = indexcount;
		prim.firstvertex = vertexstart;
		prim.vertexcount = vertexcount;
		prim.mode = primitive_mode(primitive->type);
		prim.indexed = indexcount > 0;

		primitives.push_back(prim);

		vertexstart += vertexcount;
		indexstart += indexcount;
	}

	Mesh *mesh = new Mesh { &vertices, indices, primitives }; 
	meshes.push_back(mesh);
}

static struct skin_t load_skin(const cgltf_skin *gltfskin)
{
	struct skin_t skinny;

	skinny.name = gltfskin->name ? gltfskin->name : "unnamed";

	// import the inverse bind matrices of each skeleton joint
	const cgltf_accessor *accessor = gltfskin->inverse_bind_matrices;
	size_t ncomponents = cgltf_num_components(accessor->type);
	float *buf = new float[accessor->count * ncomponents];
	cgltf_accessor_unpack_floats(accessor, buf, ncomponents * accessor->count);
	for (size_t i = 0, offset = 0; i < accessor->count; i++, offset += ncomponents) {
		if (ncomponents == 16) {
			skinny.inversebinds.push_back(glm::make_mat4(buf+offset));
		} else {
			printf("invalid matrix size\n");
			skinny.inversebinds.push_back(glm::mat4(1.f));
		}
	}
	delete buf;

	return skinny;
}

static void append_buffer(const cgltf_accessor *accessor,  std::vector<uint8_t> &buffer)
{
	const cgltf_buffer_view *view = accessor->buffer_view;
	size_t type_size = cgltf_component_size(accessor->component_type) * cgltf_num_components(accessor->type); // example vec3 = 4 byte * 3 float

	uint8_t *data = (uint8_t*)view->buffer->data + view->offset;
	if (view->stride > 0) { // attribute data is interleaved
		for (size_t stride = accessor->offset; stride < view->size; stride += view->stride) {
			buffer.insert(buffer.end(), data + stride, data + stride + type_size);
		}
	} else { // attribute data is tightly packed
		buffer.insert(buffer.end(), data, data  + view->size);
	}
}

static glm::mat4 local_node_transform(const struct node_t *n)
{
	return glm::translate(glm::mat4(1.f), n->translation) * glm::mat4(n->rotation) * glm::scale(glm::mat4(1.f), n->scale) * n->transform;
}

glm::mat4 global_node_transform(const struct node_t *n)
{
	glm::mat4 m = local_node_transform(n);
	struct node_t *p = n->parent;
	while (p) {
		m = local_node_transform(p) * m;
		p = p->parent;
	}

	return m;
}

static struct collision_trimesh_t load_collision_trimesh(const cgltf_mesh *gltfmesh)
{
	struct collision_trimesh_t meshie;

	meshie.name = gltfmesh->name;

	std::vector<uint8_t> indices;
	std::vector<uint8_t> positions;
  	uint32_t vertexstart = 0;
  	uint32_t indexstart = 0;
	for (int i = 0; i < gltfmesh->primitives_count; i++) {
		const cgltf_primitive *primitive = &gltfmesh->primitives[i];
		if (primitive->indices) { 
			append_buffer(primitive->indices, indices);
			const void *data = &(indices[indexstart]);
			const uint16_t *buf = static_cast<const uint16_t*>(data);
			for (size_t index = 0; index < primitive->indices->count; index++) {
				meshie.indices.push_back(buf[index]);
			}

			indexstart = indices.size();
		}
		for (int j = 0; j < primitive->attributes_count; j++) {
  			const cgltf_attribute *attribute = &primitive->attributes[j];
			if (attribute->type == cgltf_attribute_type_position) {
				append_buffer(attribute->data, positions);
				size_t stride = cgltf_num_components(attribute->data->type);
				const void *data = &(positions[vertexstart]);
				const float *buf = static_cast<const float*>(data);
				for (size_t v = 0; v < attribute->data->count; v++) {
					glm::vec3 position = glm::make_vec3(&buf[v * stride]);
					meshie.positions.push_back(position);
				}
			
				vertexstart = positions.size();
			}
		}
	}

	return meshie;
}

static struct collision_hull_t load_collision_hull(const cgltf_mesh *gltfmesh)
{
	struct collision_hull_t hull;
	
	hull.name = gltfmesh->name;

	std::vector<uint8_t> points;
  	uint32_t vertexstart = 0;
	for (int i = 0; i < gltfmesh->primitives_count; i++) {
		const cgltf_primitive *primitive = &gltfmesh->primitives[i];
		for (int j = 0; j < primitive->attributes_count; j++) {
  			const cgltf_attribute *attribute = &primitive->attributes[j];
			if (attribute->type == cgltf_attribute_type_position) {
				append_buffer(attribute->data, points);
				size_t stride = cgltf_num_components(attribute->data->type);
				const void *data = &(points[vertexstart]);
				const float *buf = static_cast<const float*>(data);
				for (size_t v = 0; v < attribute->data->count; v++) {
					glm::vec3 position = glm::make_vec3(&buf[v * stride]);
					hull.points.push_back(position);
				}
			
				vertexstart = points.size();
			}
		}
	}

	return hull;
}

static void print_gltf_error(cgltf_result error)
{
	switch (error) {
	case cgltf_result_data_too_short:
		LOG(ERROR, "GLTF") << "data too short";
		break;
	case cgltf_result_unknown_format:
		LOG(ERROR, "GLTF") << "unknown format";
		break;
	case cgltf_result_invalid_json:
		LOG(ERROR, "GLTF") << "invalid json";
		break;
	case cgltf_result_invalid_gltf:
		LOG(ERROR, "GLTF") << "invalid GLTF";
		break;
	case cgltf_result_invalid_options:
		LOG(ERROR, "GLTF") << "invalid options";
		break;
	case cgltf_result_file_not_found:
		LOG(ERROR, "GLTF") << "file not found";
		break;
	case cgltf_result_io_error:
		LOG(ERROR, "GLTF") << "io error";
		break;
	case cgltf_result_out_of_memory:
		LOG(ERROR, "GLTF") << "out of memory";
		break;
	case cgltf_result_legacy_gltf:
		LOG(ERROR, "GLTF") << "legacy GLTF";
		break;
	};
}
