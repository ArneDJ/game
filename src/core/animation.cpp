#include <iostream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

#include "animation.h"
	
Animator::Animator(const std::string &skeletonpath, const std::string &animationpath)
{
	load_skeleton(skeletonpath);
	load_animation(animationpath);

	if (skeleton.num_joints() != animation.num_tracks()) {
		printf("animator error: skeleton joints and animation tracks do not match\n");
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
	sampling_job.ratio = controller.time_ratio();
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

void Animator::print_transforms(void)
{
	printf("------------ NEW FRAME TRANSFORMS ---------\n");
	//std::vector<ozz::math::Float4x4> models;
	for (const auto &model : models) {
  		//SimdFloat4 cols[4];
		for (int i = 0; i < 4; i++) {
			//printf("%f, %f, %f, %f\n", model.cols[i].x, models.cols[i].y, models.cols[i].z, models.cols[i].w);
			ozz::math::SimdFloat4 column = model.cols[i];
			printf("%f, ", ozz::math::GetX(column));
			printf("%f, ", ozz::math::GetY(column));
			printf("%f, ", ozz::math::GetZ(column));
			printf("%f\n, ", ozz::math::GetW(column));
		}
		printf("\n");
	}
	printf("------------ END FRAME TRANSFORMS ---------\n");
}

bool Animator::load_skeleton(const std::string &filepath)
{
	//const char *filename = "modules/media/animations/skeleton.ozz";
	ozz::io::File file(filepath.c_str(), "rb");

	// Checks file status, which can be closed if filepath.c_str() is invalid.
	if (!file.opened()) {
		ozz::log::Err() << "Cannot open file " << filepath.c_str() << "." << std::endl;
		return false;
	}

	ozz::io::IArchive archive(&file);

	if (!archive.TestTag<ozz::animation::Skeleton>()) {
		ozz::log::Err() << "Archive doesn't contain the expected object type." <<
		std::endl;
		return false;
	}

	archive >> skeleton;

	return true;
}

bool Animator::load_animation(const std::string &filepath)
{
	//assert(filename && animation);
	ozz::log::Out() << "Loading animation archive: " << filepath.c_str() << "."
	  << std::endl;
	ozz::io::File file(filepath.c_str(), "rb");
	if (!file.opened()) {
		ozz::log::Err() << "Failed to open animation file " << filepath.c_str() << "."
		    << std::endl;
		return false;
	}
	ozz::io::IArchive archive(&file);
	if (!archive.TestTag<ozz::animation::Animation>()) {
		ozz::log::Err() << "Failed to load animation instance from file "
		    << filepath.c_str() << "." << std::endl;
		return false;
	}

	// Once the tag is validated, reading cannot fail.
	archive >> animation;

	return true;
}

PlaybackController::PlaybackController(void)
    : time_ratio_(0.f),
      previous_time_ratio_(0.f),
      playback_speed_(1.f),
      play_(true),
      loop_(true) 
{
}

void PlaybackController::update(const ozz::animation::Animation& _animation, float _dt) 
{
	float new_time = time_ratio_;

	if (play_) {
		new_time = time_ratio_ + _dt * playback_speed_ / _animation.duration();
	}

	// Must be called even if time doesn't change, in order to update previous
	// frame time ratio. Uses set_time_ratio function in order to update
	// previous_time_ an wrap time value in the unit interval (depending on loop
	// mode).
	set_time_ratio(new_time);
}

void PlaybackController::set_time_ratio(float _ratio) 
{
	previous_time_ratio_ = time_ratio_;
	if (loop_) {
		// Wraps in the unit interval [0:1], even for negative values (the reason
		// for using floorf).
		time_ratio_ = _ratio - floorf(_ratio);
	} else {
		// Clamps in the unit interval [0:1].
		//time_ratio_ = ozz::math::Clamp(0.f, _ratio, 1.f);
		time_ratio_ = glm::clamp(_ratio, 0.f, 1.f);
	}
}

// Gets animation current time.
float PlaybackController::time_ratio(void) const 
{ 
	return time_ratio_; 
}

// Gets animation time of last update.
float PlaybackController::previous_time_ratio(void) const 
{
	return previous_time_ratio_;
}

void PlaybackController::reset(void) 
{
	previous_time_ratio_ = time_ratio_ = 0.f;
	playback_speed_ = 1.f;
	play_ = true;
}

