/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

#include "corelayer.h"
#include "manager.h"
#include "noncopyable.h"
#include "audio/resampler.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "audiodevice.h"

#include <cmath>
#include <vector>

namespace jami {

// AudioLayer implementation.
CoreLayer::CoreLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardring())
    , playbackBuff_(0, audioFormat_)
{}

CoreLayer::~CoreLayer()
{
    stopStream();
}

std::vector<std::string>
CoreLayer::getCaptureDeviceList() const
{
    auto list = getDeviceList(true);
    std::vector<std::string> ret;
    ret.reserve(list.size());
    for (auto& x : list)
        ret.emplace_back(std::move(x.name_));
    return ret;
}

std::vector<std::string>
CoreLayer::getPlaybackDeviceList() const
{
    auto list = getDeviceList(false);
    std::vector<std::string> ret;
    ret.reserve(list.size());
    for (auto& x : list)
        ret.emplace_back(std::move(x.name_));
    return ret;
}

int
CoreLayer::getAudioDeviceIndex(const std::string& name, DeviceType type) const
{
    int i = 0;
    for (const auto& device : getDeviceList(type == DeviceType::CAPTURE)) {
        if (device.name_ == name)
            return i;
        i++;
    }
    return 0;
}

std::string
CoreLayer::getAudioDeviceName(int index, DeviceType type) const
{
    return "";
}

void
CoreLayer::initAudioLayerIO()
{
    // OS X uses Audio Units for output. Steps:
    // 1) Create a description.
    // 2) Find the audio unit that fits that.
    // 3) Set the audio unit callback.
    // 4) Initialize everything.
    // 5) Profit...
    JAMI_DBG("INIT AUDIO IO");

    //get capture divice
    auto captureList = getDeviceList(true);
    AudioDeviceID inputDeviceID = captureList[indexIn_].id_;
    //get playback device
    auto playbackList = getDeviceList(false);
    AudioDeviceID playbackDeviceID = playbackList[indexOut_].id_;

    AudioUnitScope outputBus = 0;
    AudioUnitScope inputBus = 1;
    AudioComponentDescription desc = {0};
    desc.componentType = kAudioUnitType_Output;
    // kAudioOutputUnitProperty_EnableIO is ON and read-only
    // for input and output SCOPE on this subtype
    desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    auto comp = AudioComponentFindNext(nullptr, &desc);
    if (comp == nullptr) {
        JAMI_ERR("Can't find default output audio component.");
        return;
    }

    checkErr(AudioComponentInstanceNew(comp, &ioUnit_));

    //set capture device
    UInt32 size = sizeof(inputDeviceID);
    auto error = AudioUnitSetProperty(ioUnit_,
                                      kAudioOutputUnitProperty_CurrentDevice,
                                      kAudioUnitScope_Global,
                                      inputBus,
                                      &inputDeviceID,
                                      size);
    //if failed get default device
    if (error != kAudioServicesNoError) {
        const AudioObjectPropertyAddress inputInfo =
        {
            kAudioHardwarePropertyDefaultInputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        auto status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                 &inputInfo,
                                                 0, NULL, &size,
                                                 &inputDeviceID);
    }

    //set playback device
    size = sizeof(playbackDeviceID);
    error = AudioUnitSetProperty(ioUnit_,
                                 kAudioOutputUnitProperty_CurrentDevice,
                                 kAudioUnitScope_Global,
                                 outputBus,
                                 &playbackDeviceID,
                                 size);
    //if failed get default device
    if (error != kAudioServicesNoError) {
        const AudioObjectPropertyAddress outputInfo =
        {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        auto status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                 &outputInfo,
                                                 0, NULL, &size,
                                                 &playbackDeviceID);
    }
    // add listener for detecting when devices are removed
    const AudioObjectPropertyAddress aliveAddress =
    {
        kAudioDevicePropertyDeviceIsAlive,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioObjectAddPropertyListener(playbackDeviceID, &aliveAddress, &deviceIsAliveCallback, this);
    AudioObjectAddPropertyListener(inputDeviceID, &aliveAddress, &deviceIsAliveCallback, this);

    // Set stream format
    AudioStreamBasicDescription info;
    size = sizeof(info);
    checkErr(AudioUnitGetProperty(ioUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            outputBus,
            &info,
            &size));

    outSampleRate_ = info.mSampleRate;
    checkErr(AudioUnitGetProperty(ioUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            outputBus,
            &info,
            &size));

    audioFormat_ = {static_cast<unsigned int>(outSampleRate_),
                    static_cast<unsigned int>(info.mChannelsPerFrame)};

    outChannelsPerFrame_ = info.mChannelsPerFrame;

    info.mSampleRate = audioFormat_.sample_rate; // Only change sample rate.

    checkErr(AudioUnitSetProperty(ioUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            outputBus,
            &info,
            size));

    hardwareFormatAvailable(audioFormat_);

    // Setup audio formats
    size = sizeof(AudioStreamBasicDescription);
    checkErr(AudioUnitGetProperty(ioUnit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Input,
                inputBus,
                &info,
                &size));

    inSampleRate_ = info.mSampleRate;

    // Set format on output *SCOPE* in input *BUS*.
    checkErr(AudioUnitGetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  inputBus,
                                  &info,
                                  &size));

    audioInputFormat_ = {static_cast<unsigned int>(inSampleRate_),
                         static_cast<unsigned int>(info.mChannelsPerFrame)};
    hardwareInputFormatAvailable(audioInputFormat_);
    // Keep everything else and change only sample rate (or else SPLOSION!!!)
    info.mSampleRate = audioInputFormat_.sample_rate;
    // Keep some values to not ask them every time the read callback is fired up
    inChannelsPerFrame_ = info.mChannelsPerFrame;

    checkErr(AudioUnitSetProperty(ioUnit_,
                   kAudioUnitProperty_StreamFormat,
                   kAudioUnitScope_Output,
                   inputBus,
                   &info,
                   size));

    // Input buffer setup. Note that ioData is empty and we have to store data
    // in another buffer.
    UInt32 bufferSizeFrames = 0;
    size = sizeof(UInt32);
    checkErr(AudioUnitGetProperty(ioUnit_,
                kAudioDevicePropertyBufferFrameSize,
                kAudioUnitScope_Global,
                outputBus,
                &bufferSizeFrames,
                &size));

    UInt32 bufferSizeBytes = bufferSizeFrames * sizeof(Float32);
    size = offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * info.mChannelsPerFrame);
    rawBuff_.reset(new Byte[size + bufferSizeBytes * info.mChannelsPerFrame]);
    captureBuff_ = reinterpret_cast<::AudioBufferList*>(rawBuff_.get());
    captureBuff_->mNumberBuffers = info.mChannelsPerFrame;

    auto bufferBasePtr = rawBuff_.get() + size;
    for (UInt32 i = 0; i < captureBuff_->mNumberBuffers; ++i) {
        captureBuff_->mBuffers[i].mNumberChannels = 1;
        captureBuff_->mBuffers[i].mDataByteSize = bufferSizeBytes;
        captureBuff_->mBuffers[i].mData = bufferBasePtr + bufferSizeBytes * i;
    }

    // Input callback setup.
    AURenderCallbackStruct inputCall;
    inputCall.inputProc = inputCallback;
    inputCall.inputProcRefCon = this;

    checkErr(AudioUnitSetProperty(ioUnit_,
                kAudioOutputUnitProperty_SetInputCallback,
                kAudioUnitScope_Global,
                inputBus,
                &inputCall,
                sizeof(AURenderCallbackStruct)));

    // Output callback setup.
    AURenderCallbackStruct callback;
    callback.inputProc = outputCallback;
    callback.inputProcRefCon = this;

    checkErr(AudioUnitSetProperty(ioUnit_,
               kAudioUnitProperty_SetRenderCallback,
               kAudioUnitScope_Global,
               outputBus,
               &callback,
               sizeof(AURenderCallbackStruct)));
}

void
CoreLayer::startStream(AudioStreamType stream)
{
    JAMI_DBG("START STREAM");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != Status::Idle)
            return;
        status_ = Status::Started;
    }

    dcblocker_.reset();

    initAudioLayerIO();

    // Run
    checkErr(AudioUnitInitialize(ioUnit_));
    checkErr(AudioOutputUnitStart(ioUnit_));
}

void
CoreLayer::destroyAudioLayer()
{
    AudioOutputUnitStop(ioUnit_);
    AudioUnitUninitialize(ioUnit_);
    AudioComponentInstanceDispose(ioUnit_);
}

void
CoreLayer::stopStream()
{
    JAMI_DBG("STOP STREAM");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != Status::Started)
            return;
        status_ = Status::Idle;
    }

    destroyAudioLayer();

    /* Flush the ring buffers */
    flushUrgent();
    flushMain();
}

//// PRIVATE /////

OSStatus
CoreLayer::deviceIsAliveCallback(AudioObjectID inObjectID,
           UInt32 inNumberAddresses,
           const AudioObjectPropertyAddress inAddresses[],
           void* inRefCon)
{
    if (static_cast<CoreLayer*>(inRefCon)->status_ != Status::Started)
        return kAudioServicesNoError;
    static_cast<CoreLayer*>(inRefCon)->stopStream();
    static_cast<CoreLayer*>(inRefCon)->startStream();
    return kAudioServicesNoError;
}

OSStatus
CoreLayer::outputCallback(void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    static_cast<CoreLayer*>(inRefCon)->write(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
    return kAudioServicesNoError;
}

void
CoreLayer::write(AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    auto format = audioFormat_;
    format.sample_rate = outSampleRate_;
    format.nb_channels = outChannelsPerFrame_;
    format.sampleFormat = AV_SAMPLE_FMT_FLTP;
    if (auto toPlay = getPlayback(format, inNumberFrames)) {
        for (int i = 0; i < format.nb_channels; ++i) {
            std::copy_n((Float32*)toPlay->pointer()->extended_data[i], inNumberFrames, (Float32*)ioData->mBuffers[i].mData);
        }
    } else {
        for (int i = 0; i < format.nb_channels; ++i)
            std::fill_n(reinterpret_cast<Float32*>(ioData->mBuffers[i].mData), inNumberFrames, 0);
    }
}

OSStatus
CoreLayer::inputCallback(void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    static_cast<CoreLayer*>(inRefCon)->read(ioActionFlags, inTimeStamp, inBusNumber,
                                            inNumberFrames, ioData);
    return kAudioServicesNoError;
}

void
CoreLayer::read(AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    if (inNumberFrames <= 0) {
        JAMI_WARN("No frames for input.");
        return;
    }

    // Write the mic samples in our buffer
    checkErr(AudioUnitRender(ioUnit_,
            ioActionFlags,
            inTimeStamp,
            inBusNumber,
            inNumberFrames,
            captureBuff_));

    auto format = audioInputFormat_;
    format.sampleFormat = AV_SAMPLE_FMT_FLTP;
    auto inBuff = std::make_shared<AudioFrame>(format, inNumberFrames);
    if (isCaptureMuted_) {
        libav_utils::fillWithSilence(inBuff->pointer());
    } else {
        auto& in = *inBuff->pointer();
        for (unsigned i = 0; i < inChannelsPerFrame_; ++i)
            std::copy_n((Float32*)captureBuff_->mBuffers[i].mData, inNumberFrames, (Float32*)in.extended_data[i]);
    }
    putRecorded(std::move(inBuff));
}

void CoreLayer::updatePreference(AudioPreference &preference, int index, DeviceType type)
{
    switch (type) {
        case DeviceType::PLAYBACK:
            preference.setAlsaCardout(index);
            break;

        case DeviceType::CAPTURE:
            preference.setAlsaCardin(index);
            break;

        case DeviceType::RINGTONE:
            preference.setAlsaCardring(index);
            break;

        default:
            break;
    }
}

std::vector<AudioDevice>
CoreLayer::getDeviceList(bool getCapture) const
{
    std::vector<AudioDevice> ret;
    UInt32 propsize;

    AudioObjectPropertyAddress theAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    __Verify_noErr(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                &theAddress,
                                                0,
                                                nullptr,
                                                &propsize));

    std::size_t nDevices = propsize / sizeof(AudioDeviceID);
    auto devids = std::vector<AudioDeviceID>(nDevices);

    __Verify_noErr(AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                            &theAddress,
                                            0,
                                            nullptr,
                                            &propsize,
                                            devids.data()));

    for (int i = 0; i < nDevices; ++i) {
        auto dev = AudioDevice {devids[i], getCapture};
        if (dev.channels_ > 0) { // Channels < 0 if inactive.
            ret.push_back(std::move(dev));
        }
    }
    return ret;
}
} // namespace jami
