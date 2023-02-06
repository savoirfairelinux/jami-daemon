/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Groarke <philippe.groarke@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "audiodevice.h"

#if !TARGET_OS_IPHONE

namespace jami {

AudioDevice::AudioDevice(AudioDeviceID devid, bool isInput)
{
    init(devid, isInput);
}

void
AudioDevice::init(AudioDeviceID devid, bool isInput)
{
    id_ = devid;
    isInput_ = isInput;
    if (id_ == kAudioDeviceUnknown)
        return;

    name_ = getName();
    channels_ = countChannels();

    UInt32 propsize = sizeof(Float32);

    AudioObjectPropertyScope theScope = isInput_ ? kAudioDevicePropertyScopeInput
                                                 : kAudioDevicePropertyScopeOutput;

    AudioObjectPropertyAddress theAddress = {kAudioDevicePropertySafetyOffset,
                                             theScope,
                                             0}; // channel

    __Verify_noErr(AudioObjectGetPropertyData(id_, &theAddress, 0, NULL, &propsize, &safetyOffset_));

    propsize = sizeof(UInt32);
    theAddress.mSelector = kAudioDevicePropertyBufferFrameSize;

    __Verify_noErr(
        AudioObjectGetPropertyData(id_, &theAddress, 0, NULL, &propsize, &bufferSizeFrames_));

    propsize = sizeof(AudioStreamBasicDescription);
    theAddress.mSelector = kAudioDevicePropertyStreamFormat;

    __Verify_noErr(AudioObjectGetPropertyData(id_, &theAddress, 0, NULL, &propsize, &format_));
}

bool
AudioDevice::valid() const
{
    return id_ != kAudioDeviceUnknown;
}

void
AudioDevice::setBufferSize(UInt32 size)
{
    UInt32 propsize = sizeof(UInt32);

    AudioObjectPropertyScope theScope = isInput_ ? kAudioDevicePropertyScopeInput
                                                 : kAudioDevicePropertyScopeOutput;

    AudioObjectPropertyAddress theAddress = {kAudioDevicePropertyBufferFrameSize,
                                             theScope,
                                             0}; // channel

    __Verify_noErr(AudioObjectSetPropertyData(id_, &theAddress, 0, NULL, propsize, &size));

    __Verify_noErr(
        AudioObjectGetPropertyData(id_, &theAddress, 0, NULL, &propsize, &bufferSizeFrames_));
}

int
AudioDevice::countChannels() const
{
    OSStatus err;
    UInt32 propSize;
    int result = 0;

    AudioObjectPropertyScope theScope = isInput_ ? kAudioDevicePropertyScopeInput
                                                 : kAudioDevicePropertyScopeOutput;

    AudioObjectPropertyAddress theAddress = {kAudioDevicePropertyStreamConfiguration,
                                             theScope,
                                             0}; // channel

    err = AudioObjectGetPropertyDataSize(id_, &theAddress, 0, NULL, &propSize);
    if (err)
        return 0;

    AudioBufferList* buflist = (AudioBufferList*) malloc(propSize);
    err = AudioObjectGetPropertyData(id_, &theAddress, 0, NULL, &propSize, buflist);
    if (!err) {
        for (UInt32 i = 0; i < buflist->mNumberBuffers; ++i) {
            result += buflist->mBuffers[i].mNumberChannels;
        }
    }
    free(buflist);
    return result;
}

std::string
AudioDevice::getName() const
{
    char buf[256];
    UInt32 maxlen = sizeof(buf) - 1;

    AudioObjectPropertyScope theScope = isInput_ ? kAudioDevicePropertyScopeInput
                                                 : kAudioDevicePropertyScopeOutput;

    AudioObjectPropertyAddress theAddress = {kAudioDevicePropertyDeviceName, theScope, 0}; // channel

    __Verify_noErr(AudioObjectGetPropertyData(id_, &theAddress, 0, NULL, &maxlen, buf));
    return buf;
}

} // namespace jami

#endif // TARGET_OS_IPHONE
