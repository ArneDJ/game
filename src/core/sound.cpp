#include <string>
#include <map>
#include <SDL2/SDL_mixer.h>

#include "logger.h"
#include "sound.h"

void SoundManager::init()
{
	//Can be Bitwise combination of
	//MIX_Init_FAC, MIX_Init_MOD, MIX_Init_MP3, MIX_Init_OGG
	if (Mix_Init(MIX_INIT_OGG || MIX_INIT_MP3) == -1) {
		std::string err = "SDL_Mixer could not Initialize! Error: " + std::string(Mix_GetError());
		write_log(LogType::ERROR, err);
	}

	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		std::string err = "Mix_OpenAudio Error: " + std::string(Mix_GetError());
		write_log(LogType::ERROR, err);
	}
}

void SoundManager::teardown(void)
{
	//Release the audio resources

	for (auto &iter : effectsmap) {
		Mix_FreeChunk(iter.second);
	}
	for (auto &iter : musicmap) {
		Mix_FreeMusic(iter.second);
	}

	Mix_CloseAudio();
	Mix_Quit();
}
	
void SoundManager::change_volume(uint8_t volume)
{
	int channel = -1;
	Mix_Volume(channel, volume);
	Mix_VolumeMusic(volume);
}

SoundEffect SoundManager::load_sound(const std::string &fpath)
{
	SoundEffect effect;

	//Lookup audio file in the cached list
	auto iter = effectsmap.find(fpath);
	//Failed to find in cache, load
	if (iter == effectsmap.end()) {
		Mix_Chunk *chunk = Mix_LoadWAV(fpath.c_str());
		//Error Loading file
		if (chunk == nullptr) {
			std::string err = "Mix_LoadWAV Error: " + std::string(Mix_GetError());
			write_log(LogType::ERROR, err);
		}
		effect.chunk = chunk;
		effectsmap[fpath] = chunk;
	} else {
		effect.chunk = iter->second;
	}

	return effect;
}

Music SoundManager::load_music(const std::string &fpath)
{
	Music music;

	//Lookup audio file in the cached list
	auto iter = musicmap.find(fpath);
	//Failed to find in cache, load
	if (iter == musicmap.end()) {
		Mix_Music *chunk = Mix_LoadMUS(fpath.c_str());
		//Error Loading file
		if (chunk == nullptr) {
			write_log(LogType::ERROR,"Mix_LoadMUS Error: " + std::string(Mix_GetError()));
    		}

		music.chunk = chunk;
		musicmap[fpath] = chunk;
	} else {
		music.chunk = iter->second;
	}

	return music;
}

void SoundEffect::play(int nloops)
{
	//TODO: USE OLDEST CHANNEL THAT IS NOT IN USE FIRST
	//THIS WILL ERROR IF TOO MANY SOUNDS ARE ATTEMPTED AT ONCE  <-hack fix below
	if (Mix_PlayChannel(-1, chunk, nloops) == -1) {
		if (Mix_PlayChannel(0, chunk, nloops) == -1) {
			write_log(LogType::ERROR, "Mix_PlayChannel Error: " + std::string(Mix_GetError()));
		}
	}
}

void Music::play(int nloops)
{
	if (Mix_PlayMusic(chunk, nloops) == -1) {
		write_log(LogType::ERROR, "Mix_PlayMusic Error: " + std::string(Mix_GetError()));
	}
}
