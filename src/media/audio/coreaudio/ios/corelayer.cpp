/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Groarke <philippe.groarke@savoirfairelinux.com>
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "libav_utils.h"

#include <cmath>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace jami {

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
{
    stopStream();
}

std::vector<std::string>
CoreLayer::getCaptureDeviceList() const
{
    std::vector<std::string> ret;
    return ret;
}

std::vector<std::string>
CoreLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> ret;
    // No need to enumerate devices for iOS.
    // The notion of input devices can be ignored, and output devices can describe
    // input/output pairs.
    // Unavailable options like the receiver on iPad can be ignored by the client.
    ret.assign({"built_in_spk", "bluetooth", "headphones", "receiver"});

    return ret;
}

int
CoreLayer::getAudioDeviceIndex(const std::string& name, DeviceType type) const
{
    (void) name;
    (void) index;
    (void) type;
    return 0;
}

std::string
CoreLayer::getAudioDeviceName(int index, DeviceType type) const
{
    (void) index;
    (void) type;
    return "";
}

void
CoreLayer::initAudioLayerIO()
{
    JAMI_DBG("iOS CoreLayer - initializing audio session");

    AudioComponentDescription outputUnitDescription;
    outputUnitDescription.componentType             = kAudioUnitType_Output;
    outputUnitDescription.componentSubType          = kAudioUnitSubType_VoiceProcessingIO;
    outputUnitDescription.componentManufacturer     = kAudioUnitManufacturer_Apple;
    outputUnitDescription.componentFlags            = 0;
    outputUnitDescription.componentFlagsMask        = 0;

    auto comp = AudioComponentFindNext(nullptr, &outputUnitDescription);
    if (comp == nullptr) {
        JAMI_ERR("Can't find default output audio component.");
        return;
    }

    checkErr(AudioComponentInstanceNew(comp, &ioUnit_));

    UInt32 audioCategory = kAudioSessionCategory_PlayAndRecord;
    AudioSessionSetProperty(kAudioSessionProperty_AudioCategory,
                            sizeof(audioCategory),
                            &audioCategory);

    auto playBackDeviceList = getPlaybackDeviceList();
    JAMI_DBG("Setting playback device: %s", playBackDeviceList[indexOut_].c_str());
    switch(indexOut_) {
        case 0:
            UInt32 setSpeaker;
            setSpeaker = 1;
            checkErr(AudioSessionSetProperty(kAudioSessionProperty_OverrideCategoryDefaultToSpeaker,
                                             sizeof(setSpeaker),
                                             &setSpeaker));
            break;
        case 1:
        case 2:
            break;
        case 3:
            UInt32 audioRouteOverrideNone;
            audioRouteOverrideNone = kAudioSessionOverrideAudioRoute_None;
            checkErr(AudioSessionSetProperty(kAudioSessionProperty_OverrideAudioRoute,
                                             sizeof(audioRouteOverrideNone),
                                             &audioRouteOverrideNone));
            break;
        default:
            break;
    }

    setupOutputBus();
    setupInputBus();
    bindCallbacks();
}

void
CoreLayer::setupOutputBus() {
    JAMI_DBG("iOS CoreLayer - initializing output bus");

    AudioUnitScope outputBus = 0;
    UInt32 size;

    AudioStreamBasicDescription outputASBD;
    size = sizeof(outputASBD);

    Float64 outSampleRate;
    size = sizeof(outSampleRate);
    AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate,
                            &size,
                            &outSampleRate);
    outputASBD.mSampleRate = outSampleRate;
    outSampleRate_ = outputASBD.mSampleRate;

    size = sizeof(outputASBD);
    checkErr(AudioUnitGetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  outputBus,
                                  &outputASBD,
                                  &size));

    // Only change sample rate.
    outputASBD.mSampleRate = outSampleRate_;
    outputASBD.mFormatID = kAudioFormatLinearPCM;
    outputASBD.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;

    outSampleRate_ = outputASBD.mSampleRate;
    outChannelsPerFrame_ = outputASBD.mChannelsPerFrame;

    // Set output steam format
    checkErr(AudioUnitSetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  outputBus,
                                  &outputASBD,
                                  size));

    hardwareFormatAvailable({static_cast<unsigned int>(outputASBD.mSampleRate),
                            static_cast<unsigned int>(outputASBD.mChannelsPerFrame)});
}

void
CoreLayer::setupInputBus() {
    JAMI_DBG("Initializing input bus");

    AudioUnitScope inputBus = 1;
    UInt32 size;

    AudioStreamBasicDescription inputASBD;
    size = sizeof(inputASBD);

    // Enable input
    UInt32 flag = 1;
    checkErr(AudioUnitSetProperty (ioUnit_,
                                   kAudioOutputUnitProperty_EnableIO,
                                   kAudioUnitScope_Input,
                                   inputBus,
                                   &flag,
                                   sizeof(flag)));

    // Setup audio formats
    checkErr(AudioUnitGetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  inputBus,
                                  &inputASBD,
                                  &size));

    Float64 inSampleRate;
    size = sizeof(Float64);
    AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate,
                            &size,
                            &inSampleRate);
    inputASBD.mSampleRate = inSampleRate;
    inputASBD.mFormatID = kAudioFormatLinearPCM;
    inputASBD.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;


    // Set format on output *SCOPE* in input *BUS*.
    checkErr(AudioUnitGetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  inputBus,
                                  &inputASBD,
                                  &size));
    inputASBD.mSampleRate = inSampleRate;
    audioInputFormat_ = {static_cast<unsigned int>(inputASBD.mSampleRate),
                         static_cast<unsigned int>(inputASBD.mChannelsPerFrame)};
    hardwareInputFormatAvailable(audioInputFormat_);

    // Keep some values to not ask them every time the read callback is fired up
    inSampleRate_ = inputASBD.mSampleRate;
    inChannelsPerFrame_ = inputASBD.mChannelsPerFrame;

    size = sizeof(inputASBD);
    checkErr(AudioUnitSetProperty(ioUnit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  inputBus,
                                  &inputASBD,
                                  size));

    // Input buffer setup. Note that ioData is empty and we have to store data
    // in another buffer.
    flag = 0;
    AudioUnitSetProperty(ioUnit_,
                         kAudioUnitProperty_ShouldAllocateBuffer,
                         kAudioUnitScope_Output,
                         inputBus,
                         &flag,
                         sizeof(flag));

    Float32 bufferDuration;
    size = sizeof(UInt32);
    AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareIOBufferDuration,
                            &size,
                            &bufferDuration);
    UInt32 bufferSizeFrames = std::round(inSampleRate_ * bufferDuration);
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

    // Output callback setup
    callback.inputProc = outputCallback;
    callback.inputProcRefCon = this;

    checkErr(AudioUnitSetProperty(ioUnit_,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  outputBus,
                                  &callback,
                                  sizeof(AURenderCallbackStruct)));

    // Input callback setup
    AURenderCallbackStruct inputCall;
    inputCall.inputProc = inputCallback;
    inputCall.inputProcRefCon = this;

    checkErr(AudioUnitSetProperty(ioUnit_,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  inputBus,
                                  &inputCall,
                                  sizeof(AURenderCallbackStruct)));
}

void
CoreLayer::startStream()
{
    JAMI_DBG("iOS CoreLayer - Start Stream");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != Status::Idle)
            return;
        status_ = Status::Started;
    }

    dcblocker_.reset();

    initAudioLayerIO();

    // Run
    auto inputRes = AudioUnitInitialize(ioUnit_);
    auto outputRes = AudioOutputUnitStart(ioUnit_);
    if(inputRes || outputRes) {
        stopStream();
        checkErr(inputRes);
        checkErr(outputRes);
    }
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
    JAMI_DBG("iOS CoreLayer - Stop Stream");

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
    (void) ioActionFlags;
    (void) inTimeStamp;
    (void) inBusNumber;

    AudioFormat currentOutFormat {  static_cast<unsigned>(outSampleRate_),
                                    static_cast<unsigned>(outChannelsPerFrame_),
                                    AV_SAMPLE_FMT_FLTP};

    if (auto toPlay = getPlayback(currentOutFormat, inNumberFrames)) {
        const auto& frame = *toPlay->pointer();
        for (unsigned i = 0; i < frame.channels; ++i) {
            std::copy_n((Float32*)frame.extended_data[i], inNumberFrames, (Float32*)ioData->mBuffers[i].mData);
        }
    } else {
        for (int i = 0; i < currentOutFormat.nb_channels; ++i)
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
    (void) ioData;

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
    auto inBuff = std::make_unique<AudioFrame>(format, inNumberFrames);
    if (isCaptureMuted_) {
        libav_utils::fillWithSilence(inBuff->pointer());
    } else {
        auto& in = *inBuff->pointer();
        for (unsigned i = 0; i < inChannelsPerFrame_; ++i)
            std::copy_n((Float32*)captureBuff_->mBuffers[i].mData, inNumberFrames, (Float32*)in.extended_data[i]);
    }
    mainRingBuffer_->put(std::move(inBuff));
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

} // namespace jami

#pragma GCC diagnostic pop
