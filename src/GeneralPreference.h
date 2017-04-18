#pragma once
class GeneralPreference{

public:

	GeneralPreference()
	{
		recordpath_ = "";
		alwaysRecording_ = false;
		volumemic_ = 1.0;
		volumespkr_ = 1.0;
	}

	~GeneralPreference()
	{
	}
	
	
	std::string getRecordPath() const {
		return recordpath_;
	}

	//Cette methode est sans implementation dans ring
	bool setRecordPath(const std::string &r) {

		recordpath_ = r;

		//TO DO 
		//Checker is directory is writebale
		//if ok return true else retrun false

		return true;
	}

	bool getIsAlwaysRecording() const {
		return alwaysRecording_;
	}

	void setIsAlwaysRecording(bool rec) {
		alwaysRecording_ = rec;
	}

	double getVolumemic() const {
		return volumemic_;
	}
	void setVolumemic(double m) {
		volumemic_ = m;
	}

	double getVolumespkr() const {
		return volumespkr_;
	}
	void setVolumespkr(double s) {
		volumespkr_ = s;
	}

private:

	std::string recordpath_;
	bool alwaysRecording_;
	double volumemic_;
	double volumespkr_;
};