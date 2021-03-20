#include <iostream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../extern/ozz/animation/runtime/animation.h"
#include "../extern/ozz/animation/runtime/local_to_model_job.h"
#include "../extern/ozz/animation/runtime/sampling_job.h"
#include "../extern/ozz/animation/runtime/skeleton.h"
#include "../extern/ozz/base/log.h"
#include "../extern/ozz/base/maths/soa_transform.h"
#include "../extern/ozz/base/io/archive.h"
#include "../extern/ozz/base/io/stream.h"

#include "logger.h"
#include "animation.h"
	
// TODO find a more efficient conversion
//std::vector<ozz::math::Float4x4> models;
glm::mat4 ozz_to_mat4(const ozz::math::Float4x4 &matrix)
{
	glm::mat4 out;
	ozz::math::StorePtrU(matrix.cols[0], glm::value_ptr(out));
	ozz::math::StorePtrU(matrix.cols[1], glm::value_ptr(out) + 4);
	ozz::math::StorePtrU(matrix.cols[2], glm::value_ptr(out) + 8);
	ozz::math::StorePtrU(matrix.cols[3], glm::value_ptr(out) + 12);

	return out;
}

Animator::Animator(const std::string &skeletonpath, const std::string &animationpath)
{
	load_skeleton(skeletonpath);
	load_animation(animationpath);

	if (skeleton.num_joints() != animation.num_tracks()) {
		write_log(LogType::ERROR, "Animation error: skeleton joints and animation tracks do not match\n");
	}

	// Allocates runtime buffers.
	const int num_soa_joints = skeleton.num_soa_joints();
	locals.resize(num_soa_joints);
	const int num_joints = skeleton.num_joints();
	models.resize(num_joints);

	// Allocates a cache that matches animation requirements.
	cache.Resize(num_joints);
}
	
void Animator::update(float delta)
{
	// Updates current animation time.
	controller.update(animation, delta);

	// Samples optimized animation at t = animation_time_.
	ozz::animation::SamplingJob sampling_job;
	sampling_job.animation = &animation;
	sampling_job.cache = &cache;
	sampling_job.ratio = controller.time_ratio;
	sampling_job.output = ozz::make_span(locals);
	if (!sampling_job.Run()) {
		return;
	}

	// Converts from local space to model space matrices.
	ozz::animation::LocalToModelJob ltm_job;
	ltm_job.skeleton = &skeleton;
	ltm_job.input = ozz::make_span(locals);
	ltm_job.output = ozz::make_span(models);
	if (!ltm_job.Run()) {
		return;
	}
}

bool Animator::load_skeleton(const std::string &filepath)
{
	ozz::io::File file(filepath.c_str(), "rb");

	// Checks file status, which can be closed if filepath.c_str() is invalid.
	if (!file.opened()) {
		std::string err = "Animation error: cannot open skeleton file " + filepath;
		write_log(LogType::ERROR, err);
		return false;
	}

	ozz::io::IArchive archive(&file);

	if (!archive.TestTag<ozz::animation::Skeleton>()) {
		write_log(LogType::ERROR, "Animation error: archive doesn't contain the expected object type");
		return false;
	}

	archive >> skeleton;

	return true;
}

bool Animator::load_animation(const std::string &filepath)
{
	ozz::io::File file(filepath.c_str(), "rb");
	if (!file.opened()) {
		std::string err = "Animation error: cannot open animation file " + filepath;
		write_log(LogType::ERROR, err);
		return false;
	}
	ozz::io::IArchive archive(&file);
	if (!archive.TestTag<ozz::animation::Animation>()) {
		write_log(LogType::ERROR, "Animation error: failed to load animation instance from file");
		return false;
	}

	// Once the tag is validated, reading cannot fail.
	archive >> animation;

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

