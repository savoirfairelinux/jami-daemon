/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
    , is_playback_running_(false)
    , is_capture_running_(false)
    , mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{}

CoreLayer::~CoreLayer()
{
    isStarted_ = false;

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

    for (auto x : getDeviceList(true))
        ret.push_back(x.name_);

    return ret;
}

std::vector<std::string> CoreLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> ret;

    for (auto x : getDeviceList(false))
    {
        ret.push_back(x.name_);
    }

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

void CoreLayer::initAudioLayerPlayback()
{
    // OS X uses Audio Units for output. Steps:
    // 1) Create a description.
    // 2) Find the audio unit that fits that.
    // 3) Set the audio unit callback.
    // 4) Initialize everything.
    // 5) Profit...

    RING_DBG("INIT AUDIO PLAYBACK");

    AudioComponentDescription outputDesc = {0};
    outputDesc.componentType = kAudioUnitType_Output;
    outputDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
    outputDesc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent outComp = AudioComponentFindNext(NULL, &outputDesc);
    if (outComp == NULL) {
        RING_ERR("Can't find default output audio component.");
        return;
    }

    checkErr(AudioComponentInstanceNew(outComp, &outputUnit_));

    // Setup Callback.
    AURenderCallbackStruct callback;
    callback.inputProc = outputCallback;
    callback.inputProcRefCon = this;

    checkErr(AudioUnitSetProperty(outputUnit_,
               kAudioUnitProperty_SetRenderCallback,
               kAudioUnitScope_Input,
               0,
               &callback,
               sizeof(callback)));


    // Set stream format
    AudioStreamBasicDescription info;
    UInt32 size = sizeof(info);
    checkErr(AudioUnitGetProperty(outputUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            0,
            &info,
            &size));
    audioFormat_ = {(unsigned int)info.mSampleRate, (unsigned int)info.mChannelsPerFrame};
    checkErr(AudioUnitGetProperty(outputUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            0,
            &info,
            &size));

    info.mSampleRate = audioFormat_.sample_rate; // Only change sample rate.
    checkErr(AudioUnitSetProperty(outputUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            0,
            &info,
            size));

    hardwareFormatAvailable(audioFormat_);

    // Initialize
    checkErr(AudioUnitInitialize(outputUnit_));
    checkErr(AudioOutputUnitStart(outputUnit_));

    is_playback_running_ = true;
    is_capture_running_ = true;

    initAudioFormat();
}

void CoreLayer::initAudioLayerCapture()
{
    RING_DBG("INIT AUDIO INPUT");
    // HALUnit description.
    AudioComponentDescription desc;
    desc = {0};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL)
        RING_ERR("Can't find an input HAL unit that matches description.");
    checkErr(AudioComponentInstanceNew(comp, &inputUnit_));

    // HALUnit settings.
    AudioUnitScope outputBus = 0;
    AudioUnitScope inputBus = 1;
    UInt32 enableIO = 1;
    UInt32 disableIO = 0;
    UInt32 size = 0;

    checkErr(AudioUnitSetProperty(inputUnit_,
                kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Input,
                inputBus,
                &enableIO,
                sizeof(enableIO)));

    checkErr(AudioUnitSetProperty(inputUnit_,
                kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Output,
                outputBus,
                &disableIO,
                sizeof(disableIO)));

    AudioDeviceID defaultDevice = kAudioObjectUnknown;
    size = sizeof(defaultDevice);
    AudioObjectPropertyAddress defaultDeviceProperty;
    defaultDeviceProperty.mSelector = kAudioHardwarePropertyDefaultInputDevice;
    defaultDeviceProperty.mScope = kAudioObjectPropertyScopeGlobal;
    defaultDeviceProperty.mElement = kAudioObjectPropertyElementMaster;

    checkErr(AudioObjectGetPropertyData(kAudioObjectSystemObject,
                &defaultDeviceProperty,
                outputBus,
                NULL,
                &size,
                &defaultDevice));

    checkErr(AudioUnitSetProperty(inputUnit_,
                kAudioOutputUnitProperty_CurrentDevice,
                kAudioUnitScope_Global,
                outputBus,
                &defaultDevice,
                sizeof(defaultDevice)));

    // Setup audio formats
    AudioStreamBasicDescription info;
    size = sizeof(AudioStreamBasicDescription);
    checkErr(AudioUnitGetProperty(inputUnit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Input,
                inputBus,
                &info,
                &size));

    audioInputFormat_ = {(unsigned int)info.mSampleRate, (unsigned int)info.mChannelsPerFrame};
    hardwareInputFormatAvailable(audioInputFormat_);

    // Set format on output *SCOPE* in input *BUS*.
    checkErr(AudioUnitGetProperty(inputUnit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output,
                inputBus,
                &info,
                &size));

    // Keep everything else and change only sample rate (or else SPLOSION!!!)
    info.mSampleRate = audioInputFormat_.sample_rate;
    checkErr(AudioUnitSetProperty(inputUnit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output,
                inputBus,
                &info,
                size));


    // Input buffer setup. Note that ioData is empty and we have to store data
    // in another buffer.
    UInt32 bufferSizeFrames = 0;
    size = sizeof(UInt32);
    checkErr(AudioUnitGetProperty(inputUnit_,
                kAudioDevicePropertyBufferFrameSize,
                kAudioUnitScope_Global,
                outputBus,
                &bufferSizeFrames,
                &size));

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

    checkErr(AudioUnitSetProperty(inputUnit_,
                kAudioOutputUnitProperty_SetInputCallback,
                kAudioUnitScope_Global,
                outputBus,
                &inputCall,
                sizeof(inputCall)));

    // Start it up.
    checkErr(AudioUnitInitialize(inputUnit_));
    checkErr(AudioOutputUnitStart(inputUnit_));

}

void CoreLayer::startStream()
{
    RING_DBG("START STREAM");
    dcblocker_.reset();

    if (is_playback_running_ and is_capture_running_)
        return;

    initAudioLayerPlayback();
    initAudioLayerCapture();
}

void CoreLayer::destroyAudioLayer()
{
    AudioOutputUnitStop(outputUnit_);
    AudioUnitUninitialize(outputUnit_);
    AudioComponentInstanceDispose(outputUnit_);

    AudioOutputUnitStop(inputUnit_);
    AudioUnitUninitialize(inputUnit_);
    AudioComponentInstanceDispose(inputUnit_);
}

void CoreLayer::stopStream()
{
    RING_DBG("STOP STREAM");

    isStarted_ = is_playback_running_ = is_capture_running_ = false;

    destroyAudioLayer();

    /* Flush the ring buffers */
    flushUrgent();
    flushMain();
}


//// PRIVATE /////


void CoreLayer::initAudioFormat()
{
}


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

    // Checks for resampling
    AudioFormat mainBufferAudioFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    bool resample = audioFormat_.sample_rate != mainBufferAudioFormat.sample_rate;

    unsigned urgentFramesToGet = urgentRingBuffer_.availableForGet(RingBufferPool::DEFAULT_ID);

    if (urgentFramesToGet > 0) {
        RING_WARN("Getting urgent frames.");
        size_t totSample = std::min(inNumberFrames, urgentFramesToGet);

        playbackBuff_.setFormat(audioFormat_);
        playbackBuff_.resize(totSample);
        urgentRingBuffer_.get(playbackBuff_, RingBufferPool::DEFAULT_ID);

        playbackBuff_.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

        for (int i = 0; i < audioFormat_.nb_channels; ++i)
            playbackBuff_.channelToFloat((Float32*)ioData->mBuffers[i].mData, i); // Write

        Manager::instance().getRingBufferPool().discard(totSample, RingBufferPool::DEFAULT_ID);
    }

    unsigned normalFramesToGet = Manager::instance().getRingBufferPool().availableForGet(RingBufferPool::DEFAULT_ID);

    if (normalFramesToGet > 0) {

        double resampleFactor = 1.0;
        unsigned readableSamples = inNumberFrames;

        if (resample) {
            resampleFactor = static_cast<double>(audioFormat_.sample_rate) / mainBufferAudioFormat.sample_rate;
            readableSamples = std::ceil(inNumberFrames / resampleFactor);
        }
        readableSamples = std::min(readableSamples, normalFramesToGet);
        size_t nResampled = (double) readableSamples * resampleFactor;

        playbackBuff_.setFormat(mainBufferAudioFormat);
        playbackBuff_.resize(readableSamples);
        Manager::instance().getRingBufferPool().getData(
                playbackBuff_, RingBufferPool::DEFAULT_ID);
        playbackBuff_.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

        if (resample) {
            AudioBuffer resampledOutput(readableSamples, audioFormat_);
            resampler_->resample(playbackBuff_, resampledOutput);

            for (int i = 0; i < audioFormat_.nb_channels; ++i)
                resampledOutput.channelToFloat((Float32*)ioData->mBuffers[i].mData, i);

        } else {
            for (int i = 0; i < audioFormat_.nb_channels; ++i)
                playbackBuff_.channelToFloat((Float32*)ioData->mBuffers[i].mData, i);
        }
    }



    if (normalFramesToGet <= 0) {
        AudioLoop* tone = Manager::instance().getTelephoneTone();
        AudioLoop* file_tone = Manager::instance().getTelephoneFile();

        playbackBuff_.setFormat(audioFormat_);
        playbackBuff_.resize(inNumberFrames);

        if (tone) {
            tone->getNext(playbackBuff_, playbackGain_);

        }
        else if (file_tone) {
            file_tone->getNext(playbackBuff_, playbackGain_);
        }
        else {
            playbackBuff_.reset();
        }
        for (int i = 0; i < audioFormat_.nb_channels; ++i) {
            playbackBuff_.channelToFloat((Float32*)ioData->mBuffers[i].mData, i);
        }
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
    checkErr(AudioUnitRender(inputUnit_,
            ioActionFlags,
            inTimeStamp,
            inBusNumber,
            inNumberFrames,
            captureBuff_));


    AudioStreamBasicDescription info;
    UInt32 size = sizeof(AudioStreamBasicDescription);
    checkErr(AudioUnitGetProperty(inputUnit_,
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

    return ret;
}

} // namespace ring
