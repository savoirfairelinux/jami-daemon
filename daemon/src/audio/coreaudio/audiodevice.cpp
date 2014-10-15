//
//  AudioDevice.cpp
//  printDevices
//
//  Created by Philippe Groarke on 2014-10-13.
//  Copyright (c) 2014 Groarke & Co. All rights reserved.
//

#include "audiodevice.h"

namespace sfl {

void    AudioDevice::Init(AudioDeviceID devid, bool isInput)
{
	ID = devid;
	IsInput = isInput;
	if (ID == kAudioDeviceUnknown) return;

	Name = getName();
	Channels = countChannels();

	UInt32 propsize = sizeof(Float32);

	AudioObjectPropertyScope theScope = IsInput ?
		kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertySafetyOffset,
		theScope,
		0 }; // channel

	verify_noerr(AudioObjectGetPropertyData(ID,
						&theAddress,
						0,
						NULL,
						&propsize,
						&SafetyOffset));


	propsize = sizeof(UInt32);
	theAddress.mSelector = kAudioDevicePropertyBufferFrameSize;

	verify_noerr(AudioObjectGetPropertyData(ID,
						&theAddress,
						0,
						NULL,
						&propsize,
						&BufferSizeFrames));

	propsize = sizeof(AudioStreamBasicDescription);
	theAddress.mSelector = kAudioDevicePropertyStreamFormat;

	verify_noerr(AudioObjectGetPropertyData(ID,
						&theAddress,
						0,
						NULL,
						&propsize,
						&Format));
}

void    AudioDevice::SetBufferSize(UInt32 size)
{

	UInt32 propsize = sizeof(UInt32);

	AudioObjectPropertyScope theScope = IsInput ?
		kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertyBufferFrameSize,
		theScope,
		0 }; // channel

	verify_noerr(AudioObjectSetPropertyData(ID,
						&theAddress,
						0,
						NULL,
						propsize,
						&size));

	verify_noerr(AudioObjectGetPropertyData(ID,
						&theAddress,
						0,
						NULL,
						&propsize,
						&BufferSizeFrames));
}

int AudioDevice::countChannels()
{
	OSStatus err;
	UInt32 propSize;
	int result = 0;

	AudioObjectPropertyScope theScope = IsInput ?
		kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertyStreamConfiguration,
		theScope,
		0 }; // channel

	err = AudioObjectGetPropertyDataSize(ID,
					     &theAddress,
					     0,
					     NULL,
					     &propSize);
	if (err) return 0;

	AudioBufferList *buflist = (AudioBufferList *)malloc(propSize);
	err = AudioObjectGetPropertyData(ID,
					 &theAddress,
					 0,
					 NULL,
					 &propSize,
					 buflist);
	if (!err) {
		for (UInt32 i = 0; i < buflist->mNumberBuffers; ++i) {
			result += buflist->mBuffers[i].mNumberChannels;
		}
	}
	free(buflist);
	return result;
}

std::string AudioDevice::getName()
{
	char buf[256];
	UInt32 maxlen = sizeof(buf);

	AudioObjectPropertyScope theScope = IsInput ?
		kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertyDeviceName,
		theScope,
		0 }; // channel

	verify_noerr(AudioObjectGetPropertyData(ID,
						&theAddress,
						0,
						NULL,
						&maxlen,
						buf));
	std::string ret = std::string(buf);
	return ret;
}

}
