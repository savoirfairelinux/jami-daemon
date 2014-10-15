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
	mID = devid;
	mIsInput = isInput;
	if (mID == kAudioDeviceUnknown) return;

	mName = GetName();
	mChannels = CountChannels();

	UInt32 propsize = sizeof(Float32);

	AudioObjectPropertyScope theScope = mIsInput ?
		kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertySafetyOffset,
		theScope,
		0 }; // channel

	verify_noerr(AudioObjectGetPropertyData(mID,
						&theAddress,
						0,
						NULL,
						&propsize,
						&mSafetyOffset));


	propsize = sizeof(UInt32);
	theAddress.mSelector = kAudioDevicePropertyBufferFrameSize;

	verify_noerr(AudioObjectGetPropertyData(mID,
						&theAddress,
						0,
						NULL,
						&propsize,
						&mBufferSizeFrames));

	propsize = sizeof(AudioStreamBasicDescription);
	theAddress.mSelector = kAudioDevicePropertyStreamFormat;

	verify_noerr(AudioObjectGetPropertyData(mID,
						&theAddress,
						0,
						NULL,
						&propsize,
						&mFormat));
}

void    AudioDevice::SetBufferSize(UInt32 size)
{

	UInt32 propsize = sizeof(UInt32);

	AudioObjectPropertyScope theScope = mIsInput ?
		kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertyBufferFrameSize,
		theScope,
		0 }; // channel

	verify_noerr(AudioObjectSetPropertyData(mID,
						&theAddress,
						0,
						NULL,
						propsize,
						&size));

	verify_noerr(AudioObjectGetPropertyData(mID,
						&theAddress,
						0,
						NULL,
						&propsize,
						&mBufferSizeFrames));
}

int AudioDevice::CountChannels()
{
	OSStatus err;
	UInt32 propSize;
	int result = 0;

	AudioObjectPropertyScope theScope = mIsInput ?
		kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertyStreamConfiguration,
		theScope,
		0 }; // channel

	err = AudioObjectGetPropertyDataSize(mID,
					     &theAddress,
					     0,
					     NULL,
					     &propSize);
	if (err) return 0;

	AudioBufferList *buflist = (AudioBufferList *)malloc(propSize);
	err = AudioObjectGetPropertyData(mID,
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

std::string AudioDevice::GetName()
{
	char buf[256];
	UInt32 maxlen = sizeof(buf);

	AudioObjectPropertyScope theScope = mIsInput ?
		kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertyDeviceName,
		theScope,
		0 }; // channel

	verify_noerr(AudioObjectGetPropertyData(mID,
						&theAddress,
						0,
						NULL,
						&maxlen,
						buf));
	std::string ret = std::string(buf);
	return ret;
}

}
