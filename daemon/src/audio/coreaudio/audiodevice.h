//
//  AudioDevice.h
//  printDevices
//
//  Created by Philippe Groarke on 2014-10-13.
//  Copyright (c) 2014 Groarke & Co. All rights reserved.
//

#ifndef __printDevices__AudioDevice__
#define __printDevices__AudioDevice__

#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>

#include <stdio.h>
#include <string>

namespace sfl {

class AudioDevice {
public:
	AudioDevice() : mID(kAudioDeviceUnknown) { }
	AudioDevice(AudioDeviceID devid, bool isInput) { Init(devid, isInput); }

	void    Init(AudioDeviceID devid, bool isInput);

	bool    Valid() { return mID != kAudioDeviceUnknown; }

	void    SetBufferSize(UInt32 size);

public:
	AudioDeviceID                   mID;
	std::string			mName;
	bool                            mIsInput;
	int				mChannels;
	UInt32                          mSafetyOffset;
	UInt32                          mBufferSizeFrames;
	AudioStreamBasicDescription     mFormat;

private:
	int     CountChannels();
	std::string  GetName();
};

}

#endif /* defined(__printDevices__AudioDevice__) */
