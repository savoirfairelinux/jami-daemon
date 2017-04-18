#pragma once
#include <string>

class PulseAudio{
public:
	PulseAudio()
	{
		pulseDevicePlayback_ = "";
		pulseDeviceRecord_ = "";
		pulseDeviceRingtone_ = "";
	}
	~PulseAudio()
	{
	}
	std::string getPulseDevicePlayback() const {
		return pulseDevicePlayback_;
	}
	void setPulseDevicePlayback(const std::string &p) {
		pulseDevicePlayback_ = p;
	}
	std::string getPulseDeviceRecord() const {
		return pulseDeviceRecord_;
	}
	void setPulseDeviceRecord(const std::string &r) {
		pulseDeviceRecord_ = r;
	}

	std::string getPulseDeviceRingtone() const {
		return pulseDeviceRingtone_;
	}

	void setPulseDeviceRingtone(const std::string &r) {
		pulseDeviceRingtone_ = r;
	}
private:
	std::string pulseDevicePlayback_;
	std::string pulseDeviceRecord_;
	std::string pulseDeviceRingtone_;
};

