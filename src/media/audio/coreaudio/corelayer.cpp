/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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
#include <thread>
#include <atomic>

namespace ring {

// AudioLayer implementation.
CoreLayer::CoreLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardring())
    , playbackBuff_(0, audioFormat_)
    , captureBuff_(0)
    , mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{}

CoreLayer::~CoreLayer()
{
    if (captureBuff_) {
        for (UInt32 i = 0; i < captureBuff_->mNumberBuffers; ++i)
            free(captureBuff_->mBuffers[i].mData);
        free(captureBuff_);
        captureBuff_ = 0;
    }
}

std::vector<std::string> CoreLayer::getCaptureDeviceList() const
{
    std::vector<std::string> ret;

#if !TARGET_OS_IPHONE
    for (auto x : getDeviceList(true))
        ret.push_back(x.name_);
#endif

    return ret;
}

std::vector<std::string> CoreLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> ret;

#if !TARGET_OS_IPHONE
    for (auto x : getDeviceList(false))
        ret.push_back(x.name_);
#endif

    return ret;
}

int CoreLayer::getAudioDeviceIndex(const std::string& name, DeviceType type) const
{
    return 0;
}

std::string CoreLayer::getAudioDeviceName(int index, DeviceType type) const
{
    return "";
}

void CoreLayer::initAudioLayerIO()
{
    // OS X uses Audio Units for output. Steps:
    // 1) Create a description.
    // 2) Find the audio unit that fits that.
    // 3) Set the audio unit callback.
    // 4) Initialize everything.
    // 5) Profit...
    RING_DBG("INIT AUDIO IO");

    AudioUnitScope outputBus = 0;
    AudioUnitScope inputBus = 1;
    UInt32 size = sizeof(UInt32);
    AudioComponentDescription desc = {0};
    desc.componentType = kAudioUnitType_Output;
    // kAudioOutputUnitProperty_EnableIO is ON and read-only
    // for input and output SCOPE on this subtype
    desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        RING_ERR("Can't find default output audio component.");
        return;
    }

    checkErr(AudioComponentInstanceNew(comp, &ioUnit_));

    // Set stream format
    AudioStreamBasicDescription info;
    size = sizeof(info);
    checkErr(AudioUnitGetProperty(ioUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            outputBus,
            &info,
            &size));

    audioFormat_ = {(unsigned int)info.mSampleRate, (unsigned int)info.mChannelsPerFrame};

    checkErr(AudioUnitGetProperty(ioUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            outputBus,
            &info,
            &size));

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

    audioInputFormat_ = {(unsigned int)info.mSampleRate, (unsigned int)info.mChannelsPerFrame};
    hardwareInputFormatAvailable(audioInputFormat_);

    // Set format on output *SCOPE* in input *BUS*.
    checkErr(AudioUnitGetProperty(ioUnit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output,
                inputBus,
                &info,
                &size));

    // Keep everything else and change only sample rate (or else SPLOSION!!!)
    info.mSampleRate = audioInputFormat_.sample_rate;

    checkErr(AudioUnitSetProperty(ioUnit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output,
                inputBus,
                &info,
                size));

    // Input buffer setup. Note that ioData is empty and we have to store data
    // in another buffer.
#if !TARGET_OS_IPHONE
    UInt32 bufferSizeFrames = 0;
    size = sizeof(UInt32);
    checkErr(AudioUnitGetProperty(ioUnit_,
                kAudioDevicePropertyBufferFrameSize,
                kAudioUnitScope_Global,
                outputBus,
                &bufferSizeFrames,
                &size));
#else
    Float32 bufferDuration;
    UInt32 propSize = sizeof(Float32);
    AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareIOBufferDuration,
                            &propSize,
                            &bufferDuration);
    UInt32 bufferSizeFrames = audioInputFormat_.sample_rate * bufferDuration;
#endif

    UInt32 bufferSizeBytes = bufferSizeFrames * sizeof(Float32);
    size = offsetof(AudioBufferList, mBuffers[0]) +
        (sizeof(AudioBuffer) * info.mChannelsPerFrame);
    captureBuff_ = (AudioBufferList *)malloc(size);
    captureBuff_->mNumberBuffers = info.mChannelsPerFrame;

    for (UInt32 i = 0; i < captureBuff_->mNumberBuffers; ++i) {
        captureBuff_->mBuffers[i].mNumberChannels = 1;
        captureBuff_->mBuffers[i].mDataByteSize = bufferSizeBytes;
        captureBuff_->mBuffers[i].mData = malloc(bufferSizeBytes);
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

void CoreLayer::startStream()
{
    RING_DBG("START STREAM");

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

void CoreLayer::destroyAudioLayer()
{
    AudioOutputUnitStop(ioUnit_);
    AudioUnitUninitialize(ioUnit_);
    AudioComponentInstanceDispose(ioUnit_);
}

void CoreLayer::stopStream()
{
    RING_DBG("STOP STREAM");

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

OSStatus CoreLayer::outputCallback(void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    static_cast<CoreLayer*>(inRefCon)->write(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
    return kAudioServicesNoError;
}

void CoreLayer::write(AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    auto& ringBuff = getToRing(audioFormat_, inNumberFrames);
    auto& playBuff = getToPlay(audioFormat_, inNumberFrames);

    auto& toPlay = ringBuff.frames() > 0 ? ringBuff : playBuff;

    if (toPlay.frames() == 0) {
        for (int i = 0; i < audioFormat_.nb_channels; ++i)
            memset((Float32*)ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
    }
    else {
        for (int i = 0; i < audioFormat_.nb_channels; ++i)
            toPlay.channelToFloat((Float32*)ioData->mBuffers[i].mData, i);
    }
}

OSStatus CoreLayer::inputCallback(void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    static_cast<CoreLayer*>(inRefCon)->read(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
    return kAudioServicesNoError;
}

void CoreLayer::read(AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    if (inNumberFrames <= 0) {
        RING_WARN("No frames for input.");
        return;
    }

    // Write the mic samples in our buffer
    checkErr(AudioUnitRender(ioUnit_,
            ioActionFlags,
            inTimeStamp,
            inBusNumber,
            inNumberFrames,
            captureBuff_));

    AudioStreamBasicDescription info;
    UInt32 size = sizeof(AudioStreamBasicDescription);
    checkErr(AudioUnitGetProperty(ioUnit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output,
                inBusNumber,
                &info,
                &size));

    // Add them to Ring ringbuffer.
    const AudioFormat mainBufferFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    bool resample = info.mSampleRate != mainBufferFormat.sample_rate;

    // FIXME: Performance! There *must* be a better way. This is testing only.
    AudioBuffer inBuff(inNumberFrames, audioInputFormat_);

    for (int i = 0; i < info.mChannelsPerFrame; ++i) {
        Float32* data = (Float32*)captureBuff_->mBuffers[i].mData;
        for (int j = 0; j < inNumberFrames; ++j) {
            (*inBuff.getChannel(i))[j] = (AudioSample)((data)[j] / .000030517578125f);
        }
    }

    if (resample) {
        //RING_WARN("Resampling Input.");

        //FIXME: May be a multiplication, check alsa vs pulse implementation.

        int outSamples = inNumberFrames / (static_cast<double>(audioInputFormat_.sample_rate) / mainBufferFormat.sample_rate);
        AudioBuffer out(outSamples, mainBufferFormat);
        inputResampler_->resample(inBuff, out);
        dcblocker_.process(out);
        mainRingBuffer_->put(out);
    } else {
        dcblocker_.process(inBuff);
        mainRingBuffer_->put(inBuff);
    }
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

std::vector<AudioDevice> CoreLayer::getDeviceList(bool getCapture) const
{
    std::vector<AudioDevice> ret;
#if !TARGET_OS_IPHONE
    UInt32 propsize;

    AudioObjectPropertyAddress theAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };

    verify_noerr(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                            &theAddress,
                            0,
                            NULL,
                            &propsize));

    size_t nDevices = propsize / sizeof(AudioDeviceID);
    AudioDeviceID *devids = new AudioDeviceID[nDevices];

    verify_noerr(AudioObjectGetPropertyData(kAudioObjectSystemObject,
                        &theAddress,
                        0,
                        NULL,
                        &propsize,
                        devids));

    for (int i = 0; i < nDevices; ++i) {
        AudioDevice dev(devids[i], getCapture);
        if (dev.channels_ > 0) { // Channels < 0 if inactive.
            ret.push_back(dev);
        }
    }
    delete[] devids;
#endif
    return ret;
}

} // namespace ring
