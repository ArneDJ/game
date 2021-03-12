
class SoundEffect {
public:
	friend class SoundManager;
	///Plays the sound file
	///@param numOfLoops: If == -1, loop forever,
	///otherwise loop of number times provided + 1
	void play(int nloops = 0);
private:
	Mix_Chunk *chunk = nullptr;
};

class Music {
public:
	friend class SoundManager;
	///Plays the music file
	///@param numOfLoops: If == -1, loop forever,
	///otherwise loop of number times provided
	void play(int nloops = -1);

	static void pause(void) { Mix_PauseMusic(); };
	static void stop(void) { Mix_HaltMusic(); };
	static void resume(void) { Mix_ResumeMusic(); };
private:
	Mix_Music *chunk = nullptr;
};

class SoundManager
{
public:
	void init(void);
	void teardown(void);
	void change_volume(uint8_t volume);
	SoundEffect load_sound(const std::string &fpath);
	Music load_music(const std::string &fpath);
private:
	// TODO replace string with 64 bit ID
	std::map<std::string, Mix_Chunk*> effectsmap;
	std::map<std::string, Mix_Music*> musicmap;
};

