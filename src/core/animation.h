
glm::mat4 ozz_to_mat4(const ozz::math::Float4x4 &matrix);

class PlaybackController {
public:
	PlaybackController(void);
	// Sets animation current time.
	void set_time_ratio(float _time);
	// Gets animation current time.
	float time_ratio(void) const;
	// Gets animation time ratio of last update. Useful when the range between
	// previous and current frame needs to pe processed.
	float previous_time_ratio(void) const;
	// Sets playback speed.
	void set_playback_speed(float _speed) { playback_speed_ = _speed; }
	// Gets playback speed.
	float playback_speed(void) const { return playback_speed_; }
	// Sets loop modes. If true, animation time is always clamped between 0 and 1.
	void set_loop(bool _loop) { loop_ = _loop; }
	// Gets loop mode.
	bool loop(void) const { return loop_; }
	// Updates animation time if in "play" state, according to playback speed and
	// given frame time _dt.
	// Returns true if animation has looped during update
	void update(const ozz::animation::Animation& _animation, float _dt);
	// Resets all parameters to their default value.
	void reset(void);
	// Do controller Gui.
	// Returns true if animation time has been changed.
	//bool OnGui(const ozz::animation::Animation& _animation, ImGui* _im_gui,
	//bool _enabled = true, bool _allow_set_time = true);
private:
	// Current animation time ratio, in the unit interval [0,1], where 0 is the
	// beginning of the animation, 1 is the end.
	float time_ratio_;
	// Time ratio of the previous update.
	float previous_time_ratio_;
	// Playback speed, can be negative in order to play the animation backward.
	float playback_speed_;
	// Animation play mode state: play/pause.
	bool play_;
	// Animation loop mode.
	bool loop_;
};

// skeleton animation
class Animator {
public:
	// buffer of model space matrices.
	// send this to vertex shader for skinning
	std::vector<ozz::math::Float4x4> models;
public:
	Animator(const std::string &skeletonpath, const std::string &animationpath); // TODO seperate animation and skeleton
	void update(float delta);
	void print_transforms(void);
private:
	ozz::animation::Skeleton skeleton;
	ozz::animation::Animation animation;
	// buffer of local transforms as sampled from animation.
	std::vector<ozz::math::SoaTransform> locals;
	// Sampling cache.
	ozz::animation::SamplingCache cache;
	// Playback animation controller. This is a utility class that helps with
	// controlling animation playback time.
	PlaybackController controller;
private:
	bool load_skeleton(const std::string &filepath);
	bool load_animation(const std::string &filepath);
};
