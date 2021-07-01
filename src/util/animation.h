
namespace util {

glm::mat4 ozz_to_mat4(const ozz::math::Float4x4 &matrix);

class PlaybackController {
public:
	// Current animation time ratio, in the unit interval [0,1], where 0 is the
	// beginning of the animation, 1 is the end.
	float time_ratio;
public:
	PlaybackController();
	// Sets animation current time.
	void set_time_ratio(float ratio);
	// Sets loop modes. If true, animation time is always clamped between 0 and 1.
	void set_loop(bool loop) { m_looping = loop; }
	// Gets loop mode.
	bool loop() const { return m_looping; }
	// Updates animation time if in "play" state, according to playback speed and
	// given frame time _dt.
	// Returns true if animation has looped during update
	void update(const ozz::animation::Animation& _animation, float _dt);
	// Resets all parameters to their default value.
	void reset();
	// Do controller Gui.
	// Returns true if animation time has been changed.
	//bool OnGui(const ozz::animation::Animation& _animation, ImGui* _im_gui,
	//bool _enabled = true, bool _allow_set_time = true);
private:
	// Time ratio of the previous update.
	float m_previous_time_ratio;
	// Playback speed, can be negative in order to play the animation backward.
	float m_playback_speed;
	// Animation play mode state: play/pause.
	bool m_playing;
	// Animation loop mode.
	bool m_looping;
};

// Sampler structure contains all the data required to sample a single
// animation.
class AnimationSampler {
public:
	// Playback animation controller. This is a utility class that helps with
	// controlling animation playback time.
	PlaybackController controller;
	// Blending weight for the layer.
	float weight = 1.f;
	// Runtime animation.
	ozz::animation::Animation animation;
	// Sampling cache.
	ozz::animation::SamplingCache cache;
	// Buffer of local transforms as sampled from animation_.
	std::vector<ozz::math::SoaTransform> locals;
public: 
	bool load(const std::string &filepath);
};

// skeleton animation
class Animator {
public:
	std::map<uint32_t, std::unique_ptr<AnimationSampler>> samplers;
	std::vector<ozz::math::Float4x4> models;
	// Buffer of local transforms which stores the blending result.
	std::vector<ozz::math::SoaTransform> blended_locals;
	ozz::animation::Skeleton skeleton;
public:
	Animator(const std::string &skeletonpath, const std::vector<std::pair<uint32_t, std::string>> &animationpaths);
	void update(float delta, uint32_t first, uint32_t second, float mix);
	bool is_valid() const { return m_valid; }
private:
	// Blending job bind pose threshold.
	float m_threshold = 0.1f;
	bool m_valid = true;
private:
	bool load_skeleton(const std::string &filepath);
};

};
