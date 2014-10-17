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
	AudioDevice() : ID(kAudioDeviceUnknown) { }
	AudioDevice(AudioDeviceID devid, bool isInput) { Init(devid, isInput); }

	void    Init(AudioDeviceID devid, bool isInput);

	bool    Valid() { return ID != kAudioDeviceUnknown; }

	void    SetBufferSize(UInt32 size);

public:
	AudioDeviceID                   ID;
	std::string						Name;
	bool                            IsInput;
	int								Channels;
	UInt32                          SafetyOffset;
	UInt32                          BufferSizeFrames;
	AudioStreamBasicDescription     Format;

private:
	int     countChannels();
	std::string  getName();
};

}

#endif /* defined(__printDevices__AudioDevice__) */
