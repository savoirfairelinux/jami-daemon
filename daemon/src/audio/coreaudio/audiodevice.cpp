/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "audiodevice.h"

namespace sfl {

AudioDevice::AudioDevice(AudioDeviceID devid, bool isInput)
{
    init(devid, isInput);
}

void    AudioDevice::init(AudioDeviceID devid, bool isInput)
{
    id_ = devid;
    isInput_ = isInput;
    if (id_ == kAudioDeviceUnknown) return;

    name_ = getName();
    channels_ = countChannels();

    UInt32 propsize = sizeof(Float32);

    AudioObjectPropertyScope theScope = isInput_ ?
        kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

    AudioObjectPropertyAddress theAddress = {
        kAudioDevicePropertySafetyOffset,
        theScope,
        0 }; // channel

    verify_noerr(AudioObjectGetPropertyData(id_,
                        &theAddress,
                        0,
                        NULL,
                        &propsize,
                        &safetyOffset_));


    propsize = sizeof(UInt32);
    theAddress.mSelector = kAudioDevicePropertyBufferFrameSize;

    verify_noerr(AudioObjectGetPropertyData(id_,
                        &theAddress,
                        0,
                        NULL,
                        &propsize,
                        &bufferSizeFrames_));

    propsize = sizeof(AudioStreamBasicDescription);
    theAddress.mSelector = kAudioDevicePropertyStreamFormat;

    verify_noerr(AudioObjectGetPropertyData(id_,
                        &theAddress,
                        0,
                        NULL,
                        &propsize,
                        &format_));
}

bool    AudioDevice::valid()
{
    return id_ != kAudioDeviceUnknown;
}

void    AudioDevice::setBufferSize(UInt32 size)
{

    UInt32 propsize = sizeof(UInt32);

    AudioObjectPropertyScope theScope = isInput_ ?
        kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

    AudioObjectPropertyAddress theAddress = {
        kAudioDevicePropertyBufferFrameSize,
        theScope,
        0 }; // channel

    verify_noerr(AudioObjectSetPropertyData(id_,
                        &theAddress,
                        0,
                        NULL,
                        propsize,
                        &size));

    verify_noerr(AudioObjectGetPropertyData(id_,
                        &theAddress,
                        0,
                        NULL,
                        &propsize,
                        &bufferSizeFrames_));
}

int AudioDevice::countChannels()
{
    OSStatus err;
    UInt32 propSize;
    int result = 0;

    AudioObjectPropertyScope theScope = isInput_ ?
        kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

    AudioObjectPropertyAddress theAddress = {
        kAudioDevicePropertyStreamConfiguration,
        theScope,
        0 }; // channel

    err = AudioObjectGetPropertyDataSize(id_,
                         &theAddress,
                         0,
                         NULL,
                         &propSize);
    if (err) return 0;

    AudioBufferList *buflist = (AudioBufferList *)malloc(propSize);
    err = AudioObjectGetPropertyData(id_,
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

std::string AudioDevice::getName() const
{
    char buf[256];
    UInt32 maxlen = sizeof(buf) - 1;

    AudioObjectPropertyScope theScope = isInput_ ?
        kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

    AudioObjectPropertyAddress theAddress = {
        kAudioDevicePropertyDeviceName,
        theScope,
        0 }; // channel

    verify_noerr(AudioObjectGetPropertyData(id_,
                        &theAddress,
                        0,
                        NULL,
                        &maxlen,
                        buf));
    std::string ret = std::string(buf);
    return ret;
}

}
