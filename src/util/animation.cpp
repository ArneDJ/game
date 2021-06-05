#include <iostream>
#include <vector>
#include <map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

#include "../extern/aixlog/aixlog.h"

#include "animation.h"

namespace UTIL {
	
glm::mat4 ozz_to_mat4(const ozz::math::Float4x4 &matrix)
{
	glm::mat4 out;
	ozz::math::StorePtrU(matrix.cols[0], glm::value_ptr(out));
	ozz::math::StorePtrU(matrix.cols[1], glm::value_ptr(out) + 4);
	ozz::math::StorePtrU(matrix.cols[2], glm::value_ptr(out) + 8);
	ozz::math::StorePtrU(matrix.cols[3], glm::value_ptr(out) + 12);

	return out;
}
	
bool AnimationSampler::load(const std::string &filepath)
{
	ozz::io::File file(filepath.c_str(), "rb");
	if (!file.opened()) {
		LOG(ERROR, "Animation") << "cannot open animation file" + filepath;
		return false;
	}
	ozz::io::IArchive archive(&file);
	if (!archive.TestTag<ozz::animation::Animation>()) {
		LOG(ERROR, "Animation") << "failed to load animation instance file";
		return false;
	}

	// Once the tag is validated, reading cannot fail.
	archive >> animation;

	return true;
}
	
Animator::Animator(const std::string &skeletonpath, const std::vector<std::pair<uint32_t, std::string>> &animationpaths)
{
	valid = load_skeleton(skeletonpath);
	const int num_soa_joints = skeleton.num_soa_joints();
	const int num_joints = skeleton.num_joints();

	for (const auto &path : animationpaths) {
		auto sampler = std::make_unique<AnimationSampler>();
		sampler->load(path.second);
		if (num_joints != sampler->animation.num_tracks()) {
			LOG(ERROR, "Animation") << "skeleton joints of " + skeletonpath + " and animation tracks of " + path.second + " mismatch";
		}
		// Allocates sampler runtime buffers.
		sampler->locals.resize(num_soa_joints);

		// Allocates a cache that matches animation requirements.
		sampler->cache.Resize(num_joints);

		samplers[path.first] = std::move(sampler);
	}

	// Allocates local space runtime buffers of blended data.
	blended_locals.resize(num_soa_joints);

	// Allocates model space runtime buffers of blended data.
	models.resize(num_joints);
}
	
void Animator::update(float delta, uint32_t first, uint32_t second, float mix)
{
	// update blend ratio
	for (auto it = samplers.begin(); it != samplers.end(); it++) {
		auto sampler = it->second.get();
		if (it->first == first) {
			sampler->weight = 1.f - mix;
		} else if (it->first == second) {
			sampler->weight = mix;
		} else {
			sampler->weight = 0.f;
		}
	}

	// Updates and samples all animations to their respective local space
	// transform buffers.
	for (auto it = samplers.begin(); it != samplers.end(); it++) {
		auto sampler = it->second.get();

		// Updates animations time.
		sampler->controller.update(sampler->animation, delta);

		// Early out if this sampler weight makes it irrelevant during blending.
		if (sampler->weight <= 0.f) {
			continue;
		}

		// Setup sampling job.
		ozz::animation::SamplingJob sampling_job;
		sampling_job.animation = &sampler->animation;
		sampling_job.cache = &sampler->cache;
		sampling_job.ratio = sampler->controller.time_ratio;
		sampling_job.output = ozz::make_span(sampler->locals);

		// Samples animation.
		if (!sampling_job.Run()) {
			return;
		}
	}

	// Blends animations.
	// Blends the local spaces transforms computed by sampling all animations
	// (1st stage just above), and outputs the result to the local space
	// transform buffer blended_locals_

	// Prepares blending layers.
	std::vector<ozz::animation::BlendingJob::Layer> layers;
	for (auto it = samplers.begin(); it != samplers.end(); it++) {
		ozz::animation::BlendingJob::Layer layer;
		layer.transform = ozz::make_span(it->second->locals);
		layer.weight = it->second->weight;
		layers.push_back(layer);
	}

	// Setups blending job.
	ozz::animation::BlendingJob blend_job;
	blend_job.threshold = threshold;
	blend_job.layers = ozz::make_span(layers);
	blend_job.bind_pose = skeleton.joint_bind_poses();
	blend_job.output = ozz::make_span(blended_locals);

	// Blends.
	if (!blend_job.Run()) {
		return;
	}

	// Converts from local space to model space matrices.
	// Gets the output of the blending stage, and converts it to model space.

	// Setup local-to-model conversion job.
	ozz::animation::LocalToModelJob ltm_job;
	ltm_job.skeleton = &skeleton;
	ltm_job.input = ozz::make_span(blended_locals);
	ltm_job.output = ozz::make_span(models);

	// Runs ltm job.
	if (!ltm_job.Run()) {
		return;
	}
}

bool Animator::load_skeleton(const std::string &filepath)
{
	ozz::io::File file(filepath.c_str(), "rb");

	// Checks file status, which can be closed if filepath.c_str() is invalid.
	if (!file.opened()) {
		LOG(ERROR, "Animation") << "cannot open skeleton file " + filepath;
		return false;
	}

	ozz::io::IArchive archive(&file);

	if (!archive.TestTag<ozz::animation::Skeleton>()) {
		LOG(ERROR, "Animation") << "archive doesn't contain the expected object type";
		return false;
	}

	archive >> skeleton;

	return true;
}

PlaybackController::PlaybackController(void)
{
	time_ratio = 0.f;
	previous_time_ratio = 0.f;
	playback_speed = 1.f;
	playing = true;
	looping = true;
}

void PlaybackController::update(const ozz::animation::Animation& _animation, float _dt) 
{
	float new_time = time_ratio;

	if (playing) {
		new_time = time_ratio + _dt * playback_speed / _animation.duration();
	}

	// Must be called even if time doesn't change, in order to update previous
	// frame time ratio. Uses set_time_ratio function in order to update
	// previous_time_ an wrap time value in the unit interval (depending on loop
	// mode).
	set_time_ratio(new_time);
}

void PlaybackController::set_time_ratio(float ratio) 
{
	previous_time_ratio = time_ratio;
	if (looping) {
		// Wraps in the unit interval [0:1], even for negative values (the reason
		// for using floorf).
		time_ratio = ratio - floorf(ratio);
	} else {
		// Clamps in the unit interval [0:1].
		time_ratio = glm::clamp(ratio, 0.f, 1.f);
	}
}

void PlaybackController::reset(void) 
{
	previous_time_ratio = time_ratio = 0.f;
	playback_speed = 1.f;
	playing = true;
}

};
