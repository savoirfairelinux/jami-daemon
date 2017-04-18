#pragma once
#include <string>

class Alsa {

public:
	Alsa()
	{
		alsaCardin_ = 0;
		alsaCardout_ = 0;
		alsaCardring_ = 0;
		alsaPlugin_ = "";
		alsaSmplrate_ = 0;
	}

	~Alsa()
	{
	}

	int getAlsaCardin() const {
		return alsaCardin_;
	}
	void setAlsaCardin(int c) {
		alsaCardin_ = c;
	}

	int getAlsaCardout() const {
		return alsaCardout_;
	}

	void setAlsaCardout(int c) {
		alsaCardout_ = c;
	}

	int getAlsaCardring() const {
		return alsaCardring_;
	}

	void setAlsaCardring(int c) {
		alsaCardring_ = c;
	}

	std::string getAlsaPlugin() const {
		return alsaPlugin_;
	}

	void setAlsaPlugin(const std::string &p) {
		alsaPlugin_ = p;
	}

	int getAlsaSmplrate() const {
		return alsaSmplrate_;
	}
	void setAlsaSmplrate(int r) {
		alsaSmplrate_ = r;
	}

private:
	int alsaCardin_;
	int alsaCardout_;
	int alsaCardring_;
	std::string alsaPlugin_;
	int alsaSmplrate_;
};
