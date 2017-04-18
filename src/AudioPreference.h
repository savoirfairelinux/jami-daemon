#pragma once
#include <string>
#include"PulseAudioPreference.h"
#include"AlsaPreference.h"
#include"GeneralPreference.h"

constexpr static const char * const CONFIG_LABEL = "audio";

class AudioPreference {

public:

	/*AudioPreference();
	AudioLayer *createAudioLayer();
	AudioLayer *switchAndCreateAudioLayer();*/

	AudioPreference()
	{
		audioApi_ = "";
		denoise_ = false;
		agcEnabled_ = false;
		captureMuted_ = false;
		playbackMuted_ = false;

		//Nouveaux objets
		alsaPreference_ = Alsa();
		pulseAudioPreference_ = PulseAudio();
		generalPreference_ = GeneralPreference();
	}

	~AudioPreference()
	{
	}

	std::string getAudioApi() const {
		return audioApi_;
	}

	void setAudioApi(const std::string &api) {
		audioApi_ = api;
	}

	Alsa getAlsaPreference() const
	{
		return alsaPreference_;
	}
	void setAlsaPreference(Alsa alsaPref)
	{
		alsaPreference_ = alsaPref;
	}

	PulseAudio getPulseAudioPreference() const
	{
		return pulseAudioPreference_;
	}

	void  setPulseAudioPreference(PulseAudio puls)
	{
		pulseAudioPreference_ = puls;
	}

	GeneralPreference getGeneralPreference() const
	{
		return generalPreference_;
	}

	void  setGeneralPreference(GeneralPreference pref)
	{
		generalPreference_ = pref;
	}


	bool isAGCEnabled() const {
		return agcEnabled_;
	}

	void setAGCState(bool enabled) {
		agcEnabled_ = enabled;
	}

	bool getNoiseReduce() const {
		return denoise_;
	}

	void setNoiseReduce(bool enabled) {
		denoise_ = enabled;
	}

	bool getCaptureMuted() const {
		return captureMuted_;
	}

	void setCaptureMuted(bool muted) {
		captureMuted_ = muted;
	}

	bool getPlaybackMuted() const {
		return playbackMuted_;
	}

	void setPlaybackMuted(bool muted) {
		playbackMuted_ = muted;
	}

private:

	std::string audioApi_;
	bool denoise_;
	bool agcEnabled_;
	bool captureMuted_;
	bool playbackMuted_;

	//Nouveaux objets
	Alsa alsaPreference_;
	PulseAudio pulseAudioPreference_;
	GeneralPreference generalPreference_;
};