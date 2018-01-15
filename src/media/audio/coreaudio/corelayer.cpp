/*
 *  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
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

namespace ring {

// AudioLayer implementation.
CoreLayer::CoreLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardring())
    , playbackBuff_(0, audioFormat_)
    , mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{}

CoreLayer::~CoreLayer()
{}

std::vector<std::string>
CoreLayer::getCaptureDeviceList() const
{
    std::vector<std::string> ret;

#if !TARGET_OS_IPHONE
    for (const auto& x : getDeviceList(true))
        ret.push_back(x.name_);
#endif

    return ret;
}

std::vector<std::string>
CoreLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> ret;

#if !TARGET_OS_IPHONE
    for (const auto& x : getDeviceList(false))
        ret.push_back(x.name_);
#endif

    return ret;
}

int
CoreLayer::getAudioDeviceIndex(const std::string& name, DeviceType type) const
{
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
    // CoreAudio uses Audio Units for output. Steps:
    // 1) Create a description.
    // 2) Find the audio unit that fits that.
    // 3) Set the audio unit callback.
    // 4) Initialize everything.
    // 5) Profit...
    RING_DBG("INIT AUDIO IO");

    AudioComponentDescription outputUnitDescription;
    outputUnitDescription.componentType             = kAudioUnitType_Output;
    outputUnitDescription.componentSubType          = kAudioUnitSubType_VoiceProcessingIO;
    outputUnitDescription.componentManufacturer     = kAudioUnitManufacturer_Apple;
    outputUnitDescription.componentFlags            = 0;
    outputUnitDescription.componentFlagsMask        = 0;

    auto comp = AudioComponentFindNext(nullptr, &outputUnitDescription);
    if (comp == nullptr) {
        RING_ERR("Can't find default output audio component.");
        return;
    }

    checkErr(AudioComponentInstanceNew(comp, &ioUnit_));

    setupOutputBus();
    setupInputBus();
    bindCallbacks();
}

void
CoreLayer::setupOutputBus() {
    RING_DBG("Initializing output bus");

    AudioUnitScope outputBus = 0;
    UInt32 size;

    AudioStreamBasicDescription outputASBD;
    size = sizeof(outputASBD);

#if TARGET_OS_IPHONE
    Float64 outSampleRate;
    size = sizeof(outSampleRate);
    AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate,
                            &size,
                            &outSampleRate);

    outputASBD.mSampleRate = outSampleRate;
    outputASBD.mFormatID = kAudioFormatLinearPCM;
    outputASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
    outputASBD.mBitsPerChannel = 16;
    outputASBD.mBytesPerFrame = 2;
    outputASBD.mChannelsPerFrame = 1;
    outputASBD.mBytesPerPacket = 2;
    outputASBD.mFramesPerPacket = 1;
#else
    checkErr(AudioUnitGetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  outputBus,
                                  &outputASBD,
                                  &size));
#endif

    audioFormat_ = {static_cast<unsigned int>(outputASBD.mSampleRate),
                    static_cast<unsigned int>(outputASBD.mChannelsPerFrame)};

    size = sizeof(outputASBD);
    checkErr(AudioUnitGetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  outputBus,
                                  &outputASBD,
                                  &size));

    // Only change sample rate.
    outputASBD.mSampleRate = audioFormat_.sample_rate;

    // Set output steam format
    checkErr(AudioUnitSetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  outputBus,
                                  &outputASBD,
                                  size));

    hardwareFormatAvailable(audioFormat_);
}

void
CoreLayer::setupInputBus() {
    RING_DBG("Initializing input bus");

    AudioUnitScope inputBus = 1;
    UInt32 size;

    AudioStreamBasicDescription inputASBD;
    size = sizeof(inputASBD);

    // Setup audio formats
    checkErr(AudioUnitGetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  inputBus,
                                  &inputASBD,
                                  &size));

#if 1
    Float64 inSampleRate;
    size = sizeof(Float64);
    AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate,
                            &size,
                            &inSampleRate);

    inputASBD.mSampleRate = inSampleRate;
    inputASBD.mFormatID = kAudioFormatLinearPCM;
    inputASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    inputASBD.mBytesPerPacket = 2;
    inputASBD.mBytesPerFrame = 2;
    inputASBD.mFramesPerPacket = 1;
    inputASBD.mChannelsPerFrame = 1;
    inputASBD.mBitsPerChannel = 16;

    // Enable input
    UInt32 flag = 1;
    checkErr(AudioUnitSetProperty (ioUnit_,
                                   kAudioOutputUnitProperty_EnableIO,
                                   kAudioUnitScope_Input,
                                   inputBus,
                                   &flag,
                                   sizeof(flag)));
#endif

    audioInputFormat_ = {static_cast<unsigned int>(inputASBD.mSampleRate),
        static_cast<unsigned int>(inputASBD.mChannelsPerFrame)};
    hardwareInputFormatAvailable(audioInputFormat_);

    // Set format on output *SCOPE* in input *BUS*.
    checkErr(AudioUnitGetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  inputBus,
                                  &inputASBD,
                                  &size));

    // Keep everything else and change only sample rate (or else SPLOSION!!!)
    inputASBD.mSampleRate = audioInputFormat_.sample_rate;

    // Keep some values to not ask them every time the read callback is fired up
    inSampleRate_ = inputASBD.mSampleRate;
    inChannelsPerFrame_ = inputASBD.mChannelsPerFrame;

    checkErr(AudioUnitSetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  inputBus,
                                  &inputASBD,
                                  size));
#if TARGET_OS_IPHONE
    UInt32 audioCategory = kAudioSessionCategory_PlayAndRecord;
    AudioSessionSetProperty(kAudioSessionProperty_AudioCategory,
                            sizeof(audioCategory),
                            &audioCategory);

    flag = 0;
    AudioUnitSetProperty(ioUnit_,
                         kAudioUnitProperty_ShouldAllocateBuffer,
                         kAudioUnitScope_Output,
                         inputBus,
                         &flag,
                         sizeof(flag));
#endif

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
    size = sizeof(UInt32);
    AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareIOBufferDuration,
                            &size,
                            &bufferDuration);
    UInt32 bufferSizeFrames = audioInputFormat_.sample_rate * bufferDuration;
#endif
    UInt32 bufferSizeBytes = bufferSizeFrames * sizeof(Float32);
    size = offsetof(AudioBufferList, mBuffers[0]) + (sizeof(AudioBuffer) * inputASBD.mChannelsPerFrame);
    rawBuff_.reset(new Byte[size + bufferSizeBytes * inputASBD.mChannelsPerFrame]);
    captureBuff_ = reinterpret_cast<::AudioBufferList*>(rawBuff_.get());
    captureBuff_->mNumberBuffers = inputASBD.mChannelsPerFrame;

    auto bufferBasePtr = rawBuff_.get() + size;
    for (UInt32 i = 0; i < captureBuff_->mNumberBuffers; ++i) {
        captureBuff_->mBuffers[i].mNumberChannels = 1;
        captureBuff_->mBuffers[i].mDataByteSize = bufferSizeBytes;
        captureBuff_->mBuffers[i].mData =  bufferBasePtr + bufferSizeBytes * i;
    }
}

void
CoreLayer::bindCallbacks() {
    AURenderCallbackStruct callback;
    AudioUnitScope outputBus = 0;
    AudioUnitScope inputBus = 1;

    // Output callback setup.
    callback.inputProc = outputCallback;
    callback.inputProcRefCon = this;

    checkErr(AudioUnitSetProperty(ioUnit_,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  outputBus,
                                  &callback,
                                  sizeof(AURenderCallbackStruct)));

    // Input callback setup.
    callback.inputProc = inputCallback;
    callback.inputProcRefCon = this;

    checkErr(AudioUnitSetProperty(ioUnit_,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  inputBus,
                                  &callback,
                                  sizeof(AURenderCallbackStruct)));
}

void
CoreLayer::startStream()
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
    auto& ringBuff = getToRing(audioFormat_, inNumberFrames);
    auto& playBuff = getToPlay(audioFormat_, inNumberFrames);

    auto& toPlay = ringBuff.frames() > 0 ? ringBuff : playBuff;

    if (toPlay.frames() == 0) {
        for (unsigned i = 0; i < audioFormat_.nb_channels; ++i) {
            std::fill_n(reinterpret_cast<Float32*>(ioData->mBuffers[i].mData),
                        ioData->mBuffers[i].mDataByteSize / sizeof(Float32), 0);
        }
    } else {
        for (unsigned i = 0; i < audioFormat_.nb_channels; ++i) {
            toPlay.channelToFloat(reinterpret_cast<Float32*>(ioData->mBuffers[i].mData), i);
        }
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
    static_cast<CoreLayer*>(inRefCon)->read(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
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

    // Add them to Ring ringbuffer.
    const AudioFormat mainBufferFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    bool resample = inSampleRate_ != mainBufferFormat.sample_rate;

    // FIXME: Performance! There *must* be a better way. This is testing only.
    auto inBuff = AudioBuffer {inNumberFrames, audioInputFormat_};

    for (unsigned i = 0; i < inChannelsPerFrame_; ++i) {
        auto data = reinterpret_cast<Float32*>(captureBuff_->mBuffers[i].mData);
        for (unsigned j = 0; j < inNumberFrames; ++j) {
            (*inBuff.getChannel(i))[j] = static_cast<AudioSample>(data[j] * 32768);
        }
    }

    if (resample) {
        //FIXME: May be a multiplication, check alsa vs pulse implementation.
        UInt32 outSamples = inNumberFrames * (mainBufferFormat.sample_rate / static_cast<double>(audioInputFormat_.sample_rate));
        auto out = AudioBuffer {outSamples, mainBufferFormat};
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

std::vector<AudioDevice>
CoreLayer::getDeviceList(bool getCapture) const
{
    std::vector<AudioDevice> ret;
#if !TARGET_OS_IPHONE
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
#endif
    return ret;
}

} // namespace ring
