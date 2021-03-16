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

#define CGLTF_IMPLEMENTATION

#include "logger.h"
#include "image.h"
#include "texture.h"
#include "model.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

using namespace GLTF;

struct linkage {
	std::map<const cgltf_node*, struct node*> nodes;
	std::map<const cgltf_texture*, GLuint> textures;
};

static void print_gltf_error(cgltf_result error);
static void append_buffer(const cgltf_accessor *accessor,  std::vector<unsigned char> &buffer);
static struct node load_node(const cgltf_node *gltfnode);
static struct mesh load_mesh(const cgltf_mesh *gltfmesh, std::map<const cgltf_texture*, GLuint> &textures);
static struct animation load_animation(const cgltf_animation *gltfanimation);
static struct skin load_skin(const cgltf_skin *gltfskin);
//static GLuint load_texture(const cgltf_texture *gltftexture, std::string path);
static struct material load_material(const cgltf_material *material, std::map<const cgltf_texture*, GLuint> &textures);
static glm::mat4 local_node_transform(const struct node *n);
static struct collision_mesh load_collision_mesh(const cgltf_mesh *mesh);

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
	} else {
		write_log(LogType::ERROR, "GLTF Mesh import error for "  + filepath + ": ");
		print_gltf_error(result);
	}
		
	cgltf_free(data);
}

Model::~Model(void)
{
	for (struct mesh &m : meshes) {
		glDeleteBuffers(1, &m.EBO);
		glDeleteBuffers(1, &m.VBO);
		glDeleteVertexArrays(1, &m.VAO);
	}

	/*
	for (GLuint texture : textures) {
		delete_texture(texture);
	}
	*/
}

void Model::load_data(const std::string &filepath, const cgltf_data *data)
{
	struct linkage link;

	// load node data
	nodes.resize(data->nodes_count);
	for (int i = 0; i < data->nodes_count; i++) {
		nodes[i] = load_node(&data->nodes[i]);
		link.nodes[&data->nodes[i]] = &nodes[i];
	}
	// load textures
	/*
	std::string path = filepath.substr(0, filepath.find_last_of('/')+1);
	textures.resize(data->textures_count);
	for (int i = 0; i < data->textures_count; i++) {
		textures[i] = load_texture(&data->textures[i], path);
		link.textures[&data->textures[i]] = textures[i];
	}
	*/
	// load mesh data
	meshes.resize(data->meshes_count);
	for (int i = 0; i < data->meshes_count; i++) {
		std::string mesh_name = data->meshes[i].name ? data->meshes[i].name : std::to_string(i);
		if (mesh_name == "collision_mesh") {
			colmesh = load_collision_mesh(&data->meshes[i]);
		} else {
			meshes[i] = load_mesh(&data->meshes[i], link.textures);
		}
	}
	// load animation data
	/*
	animations.resize(data->animations_count);
	for (int i = 0; i < data->animations_count; i++) {
		animations[i] = load_animation(&data->animations[i]);
	}
	*/
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
	/*
	for (int i = 0; i < data->animations_count; i++) {
		for (int j = 0; j < data->animations[i].channels_count; j++) {
			struct node *n = link.nodes[data->animations[i].channels[j].target_node];
			animations[i].channels[j].target = n;
		}
	}
	*/
	for (int i = 0; i < data->skins_count; i++) {
		skins[i].root = link.nodes[data->skins[i].skeleton];
		for (int j = 0; j < data->skins[i].joints_count; j++) {
			struct node *n = link.nodes[data->skins[i].joints[j]];
			skins[i].joints.push_back(n);
		}
	}
}

/*
void Model::animate(int index, float time)
{
	if (animations.empty()) {
		printf("model does not contain animations\n");
		return;
	}
	if (index > animations.size()-1) {
		printf("no animation with index %d\n", index);
		return;
	}

	struct animation &anim = animations[index];
	time = fmod(time, anim.end);
	for (auto &channel : anim.channels) {
		if (channel.inputs.size() > channel.outputs.size()) { continue; }
		for (size_t i = 0; i < channel.inputs.size()-1; i++) {
			if ((time >= channel.inputs[i]) && (time <= channel.inputs[i + 1])) {
				float interpolation = std::max(0.f, time - channel.inputs[i]) / (channel.inputs[i+1] - channel.inputs[i]);
				interpolation = glm::clamp(interpolation, 0.f, 1.f);
				switch (channel.path) {
				case TRANSLATION: {
					glm::vec4 trans = glm::mix(channel.outputs[i], channel.outputs[i + 1], interpolation);
					channel.target->translation = glm::vec3(trans);
					break;
				}
				case SCALE: {
					glm::vec4 trans = glm::mix(channel.outputs[i], channel.outputs[i + 1], interpolation);
					channel.target->scale = glm::vec3(trans);
					break;
				}
				case ROTATION: {
					glm::quat q1;
					q1.x = channel.outputs[i].x;
					q1.y = channel.outputs[i].y;
					q1.z = channel.outputs[i].z;
					q1.w = channel.outputs[i].w;
					glm::quat q2;
					q2.x = channel.outputs[i + 1].x;
					q2.y = channel.outputs[i + 1].y;
					q2.z = channel.outputs[i + 1].z;
					q2.w = channel.outputs[i + 1].w;
					channel.target->rotation = glm::normalize(glm::slerp(q1, q2, interpolation));
					break;
				}
				}
			}
		}
	}

	// update the joint matrices
	std::vector<glm::mat4> matrices;
	for (struct skin &skeleton : skins) {
		glm::mat4 root_transform = skeleton.root ? local_node_transform(skeleton.root) : glm::mat4(1.f);
		//glm::mat4 inverse_root_transform = glm::inverse(root_transform);
		for (int i = 0; i < skeleton.joints.size(); i++) {
			struct node *joint = skeleton.joints[i];
			glm::mat4 joint_transform = global_node_transform(joint) * skeleton.inversebinds[i];
			glm::mat4 joint_matrix = joint_transform;
			matrices.push_back(joint_matrix);
		}
	}
	//glBindBuffer(GL_TEXTURE_BUFFER, joint_matrices.buffer);
	//glBufferData(GL_TEXTURE_BUFFER, matrices.size()*sizeof(glm::mat4), matrices.data(), GL_DYNAMIC_DRAW);
}
*/

void Model::display(void) const
{
	for (const struct mesh &m : meshes) {
		glBindVertexArray(m.VAO);
		for (const struct primitive &prim : m.primitives) {
			//activate_texture(GL_TEXTURE0, GL_TEXTURE_2D, prim.mat.colormap);
			if (prim.indexed) {
				glDrawElementsBaseVertex(prim.mode, prim.indexcount, GL_UNSIGNED_SHORT, (GLvoid *)((prim.firstindex)*sizeof(GLushort)), prim.firstvertex);
			} else {
				glDrawArrays(prim.mode, prim.firstvertex, prim.vertexcount);
			}
		}
	}
}

void Model::display_instanced(GLsizei count) const
{
	for (const struct mesh &m : meshes) {
		glBindVertexArray(m.VAO);
		for (const struct primitive &prim : m.primitives) {
			//activate_texture(GL_TEXTURE0, GL_TEXTURE_2D, prim.mat.colormap);
			if (prim.indexed) {
				glDrawElementsInstancedBaseVertex(prim.mode, prim.indexcount, GL_UNSIGNED_SHORT, (GLvoid *)((prim.firstindex)*sizeof(GLushort)), count, prim.firstvertex);
			} else {
				glDrawArraysInstanced(prim.mode, prim.firstvertex, prim.vertexcount, count);
			}
		}
	}
}

static struct node load_node(const cgltf_node *gltfnode)
{
	struct node n;
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

static struct mesh load_mesh(const cgltf_mesh *gltfmesh, std::map<const cgltf_texture*, GLuint> &textures)
{
	struct mesh meshie;
	meshie.name = gltfmesh->name ? gltfmesh->name : "unnamed";
	meshie.VAO = meshie.VBO = meshie.EBO = 0;

	 // index buffer
	std::vector<unsigned char> indices;
	// vertex buffers
	std::vector<unsigned char> positions;
	std::vector<unsigned char> normals;
	std::vector<unsigned char> texcoords;
	std::vector<unsigned char> joints;
	std::vector<unsigned char> weights;

	unsigned int vertexstart = 0;
	unsigned int indexstart = 0;
	for (int i = 0; i < gltfmesh->primitives_count; i++) {
		const cgltf_primitive *primitive = &gltfmesh->primitives[i];

		// import index data
		unsigned int indexcount = 0;
		if (primitive->indices) {
			indexcount = primitive->indices->count;
			append_buffer(primitive->indices, indices);
		}

		// An accessor also contains min and max properties that summarize the contents of their data. They are the component-wise minimum and maximum values of all data elements contained in the accessor. In the case of vertex positions, the min and max properties thus define the bounding box of an object.
		// import vertex data
		unsigned int vertexcount = 0;
		for (int j = 0; j < primitive->attributes_count; j++) {
			const cgltf_attribute *attribute = &primitive->attributes[j];
			switch (attribute->type) {
			case cgltf_attribute_type_position:
			vertexcount = attribute->data->count;
			append_buffer(attribute->data, positions);
			break;
			case cgltf_attribute_type_normal:
			append_buffer(attribute->data, normals);
			break;
			case cgltf_attribute_type_texcoord:
			append_buffer(attribute->data, texcoords);
			break;
			case cgltf_attribute_type_joints:
			append_buffer(attribute->data, joints);
			break;
			case cgltf_attribute_type_weights:
			append_buffer(attribute->data, weights);
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
		prim.mat = load_material(primitive->material, textures);

		meshie.primitives.push_back(prim);

		vertexstart += vertexcount;
		indexstart += indexcount;
	}

	std::vector<GLubyte> buffer;
	buffer.insert(buffer.end(), positions.begin(), positions.end());
	buffer.insert(buffer.end(), normals.begin(), normals.end());
	buffer.insert(buffer.end(), texcoords.begin(), texcoords.end());
	buffer.insert(buffer.end(), joints.begin(), joints.end());
	buffer.insert(buffer.end(), weights.begin(), weights.end());

	// https://www.khronos.org/opengl/wiki/Buffer_Object
	// In some cases, data stored in a buffer object will not be changed once it is uploaded. For example, vertex data can be static: set once and used many times.
	// For these cases, you set flags to 0 and use data as the initial upload. From then on, you simply use the data in the buffer. This requires that you have assembled all of the static data up-front.
	const GLbitfield flags = 0;

	glGenVertexArrays(1, &meshie.VAO);
	glBindVertexArray(meshie.VAO);

	glGenBuffers(1, &meshie.EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshie.EBO);
	glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, indices.size(), indices.data(), flags);

	glGenBuffers(1, &meshie.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, meshie.VBO);
	glBufferStorage(GL_ARRAY_BUFFER, buffer.size(), buffer.data(), flags);

	// positions
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	// normals
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 0, BUFFER_OFFSET(positions.size()));
	glEnableVertexAttribArray(1);
	// texcoords
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(positions.size()+normals.size()));
	glEnableVertexAttribArray(2);
	// joints
	glVertexAttribIPointer(3, 4, GL_UNSIGNED_SHORT, 0, BUFFER_OFFSET(positions.size()+normals.size()+texcoords.size()));
	glEnableVertexAttribArray(3);
	// weights
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(positions.size()+normals.size()+texcoords.size()+joints.size()));
	glEnableVertexAttribArray(4);

	return meshie;
}

static struct animation load_animation(const cgltf_animation *gltfanimation)
{
	struct animation anim;
	 
	anim.name = gltfanimation->name ? gltfanimation->name : "unnamed";

	// load channels
	anim.channels.resize(gltfanimation->channels_count);
	for (int i = 0; i < gltfanimation->channels_count; i++) {
		const cgltf_animation_channel *gltfchannel = &gltfanimation->channels[i];
		struct animationchannel channel;

		switch (gltfchannel->target_path) {
		case cgltf_animation_path_type_translation: channel.path = TRANSLATION; break;
		case cgltf_animation_path_type_rotation: channel.path = ROTATION; break;
		case cgltf_animation_path_type_scale: channel.path = SCALE; break;
		default: printf("this is impossible\n");
		}

		// sampler input
		const cgltf_animation_sampler *gltfsampler = gltfchannel->sampler;
		{
		size_t count = gltfsampler->input->count;
		float *buf = new float[count];
		cgltf_accessor_unpack_floats(gltfsampler->input, buf, count);
		for (size_t index = 0; index < count; index++) {
			float input = buf[index];
			if (input < anim.start) { anim.start = input; };
			if (input > anim.end) { anim.end = input; }
			channel.inputs.push_back(input);
		}
		delete buf;
		}
		// sampler output
		if (gltfsampler->output->type == cgltf_type_vec3) {
			size_t count = gltfsampler->output->count * 3;
			float *buf = new float[count];
			cgltf_accessor_unpack_floats(gltfsampler->output, buf, count);
			for (int index = 0; index < count; index += 3) {
				glm::vec3 t = glm::make_vec3(&buf[index]);
				channel.outputs.push_back(glm::vec4(t, 1.f));
			}
			delete buf;
		} else if (gltfsampler->output->type == cgltf_type_vec4) {
			size_t count = gltfsampler->output->count * 4;
			float *buf = new float[count];
			cgltf_accessor_unpack_floats(gltfsampler->output, buf, count);
			for (int index = 0; index < count; index += 4) {
				channel.outputs.push_back(glm::make_vec4(&buf[index]));
			}
			delete buf;
		} else {
			printf("gltf animation import warning: unknown type\n");
		}

		anim.channels[i] = channel;
	}

	return anim;
}

static struct skin load_skin(const cgltf_skin *gltfskin)
{
	struct skin skinny;

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

	/*
static GLuint load_texture(const cgltf_texture *gltftexture, std::string path)
{
	GLuint tex = 0;

	if (gltftexture->image->buffer_view) { // embedded PNG image
		const cgltf_buffer_view *view = gltftexture->image->buffer_view;
		unsigned char *buf = (unsigned char*)view->buffer->data + view->offset;
		int x, y;
		int nchannels;
		unsigned char *texels = stbi_load_from_memory(buf, view->size, &x, &y, &nchannels, 4);
		if (texels) {
			GLenum format = GL_RGBA;
			GLenum internalformat = GL_RGB5_A1;
			tex = standard_2D_texture(texels, x, y, internalformat, format, GL_UNSIGNED_BYTE);
			free(texels);
		}
	} else { // external DDS image
		std::string filepath = path + gltftexture->image->uri;
		tex = load_DDS(filepath.c_str());
	}

	return tex;
}
	*/

static struct material load_material(const cgltf_material *material, std::map<const cgltf_texture*, GLuint> &textures)
{
	struct material mat;
	mat.colormap = 0;
	mat.metalroughmap = 0;
	mat.normalmap = 0;
	mat.occlusionmap = 0;
	mat.emissivemap = 0;

	if (material) {
		if (material->has_pbr_metallic_roughness) { // PBR maps
			const cgltf_texture *base_color = material->pbr_metallic_roughness.base_color_texture.texture;
			if (base_color) { mat.colormap = textures[base_color]; }

			const cgltf_texture *metallic_roughness = material->pbr_metallic_roughness.metallic_roughness_texture.texture;
			if (metallic_roughness) { mat.metalroughmap = textures[metallic_roughness]; }
		} else if (material->has_pbr_specular_glossiness) { // blinn phong maps
			const cgltf_texture *base_color = material->pbr_specular_glossiness.diffuse_texture.texture;
			if (base_color) { mat.colormap = textures[base_color]; }

			const cgltf_texture *metallic_roughness = material->pbr_specular_glossiness.specular_glossiness_texture.texture;
			if (metallic_roughness) { mat.metalroughmap = textures[metallic_roughness]; }
		}

		const cgltf_texture *normal_texture = material->normal_texture.texture;
		if (normal_texture) { mat.normalmap = textures[normal_texture]; }

		const cgltf_texture *occlusion_texture = material->occlusion_texture.texture;
		if (occlusion_texture) { mat.normalmap = textures[occlusion_texture]; }

		const cgltf_texture *emissive_texture = material->emissive_texture.texture;
		if (emissive_texture) { mat.normalmap = textures[emissive_texture]; }
	}

	return mat;
}

static void append_buffer(const cgltf_accessor *accessor,  std::vector<unsigned char> &buffer)
{
	const cgltf_buffer_view *view = accessor->buffer_view;
	size_t type_size = cgltf_component_size(accessor->component_type) * cgltf_num_components(accessor->type); // example vec3 = 4 byte * 3 float

	unsigned char *data = (unsigned char*)view->buffer->data + view->offset;
	if (view->stride > 0) { // attribute data is interleaved
		for (size_t stride = accessor->offset; stride < view->size; stride += view->stride) {
			buffer.insert(buffer.end(), data + stride, data + stride + type_size);
		}
	} else { // attribute data is tightly packed
		buffer.insert(buffer.end(), data, data  + view->size);
	}
}

static glm::mat4 local_node_transform(const struct node *n)
{
	return glm::translate(glm::mat4(1.f), n->translation) * glm::mat4(n->rotation) * glm::scale(glm::mat4(1.f), n->scale) * n->transform;
}

glm::mat4 global_node_transform(const struct node *n)
{
	glm::mat4 m = local_node_transform(n);
	struct node *p = n->parent;
	while (p) {
		m = local_node_transform(p) * m;
		p = p->parent;
	}

	return m;
}

static struct collision_mesh load_collision_mesh(const cgltf_mesh *gltfmesh)
{
	struct collision_mesh meshie;

	std::vector<unsigned char> indices;
	std::vector<unsigned char> positions;
  	unsigned int vertexstart = 0;
  	unsigned int indexstart = 0;
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

static void print_gltf_error(cgltf_result error)
{
	switch (error) {
	case cgltf_result_data_too_short:
		write_log(LogType::ERROR, "data too short");
		break;
	case cgltf_result_unknown_format:
		write_log(LogType::ERROR, "unknown format");
		break;
	case cgltf_result_invalid_json:
		write_log(LogType::ERROR, "invalid json");
		break;
	case cgltf_result_invalid_gltf:
		write_log(LogType::ERROR, "invalid GLTF");
		break;
	case cgltf_result_invalid_options:
		write_log(LogType::ERROR, "invalid options");
		break;
	case cgltf_result_file_not_found:
		write_log(LogType::ERROR, "file not found");
		break;
	case cgltf_result_io_error:
		write_log(LogType::ERROR, "io error");
		break;
	case cgltf_result_out_of_memory:
		write_log(LogType::ERROR, "out of memory");
		break;
	case cgltf_result_legacy_gltf:
		write_log(LogType::ERROR, "legacy GLTF");
		break;
	};
}
	
