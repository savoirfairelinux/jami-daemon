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

#include "corelayer.h"
#include "logger.h"
#include "manager.h"
#include "noncopyable.h"
#include "client/configurationmanager.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "audiodevice.h"

#include <thread>
#include <atomic>

// Actual audio thread.
class CoreAudioThread {
public:
    CoreAudioThread(CoreLayer* coreAudio);
    ~CoreAudioThread();
    void start();
    bool isRunning() const;
private:
    void run();
    void initAudioLayer();
    std::thread thread_;
    CoreLayer* coreAudio_;
    std::atomic_bool running_;
};

CoreAudioThread::CoreAudioThread(CoreLayer* coreAudio)
    : thread_(), coreAudio_(coreAudio), running_(false)
{}

CoreAudioThread::~CoreAudioThread()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void CoreAudioThread::start()
{
    running_ = true;
    thread_ = std::thread(&CoreAudioThread::run, this);
}

bool CoreAudioThread::isRunning() const
{
    return _running;
}

void CoreAudioThread::initAudioLayer()
{
    // OS X uses Audio Units for output. Steps:
    // 1) Create a description.
    // 2) Find the audio unit that fits that.
    // 3) Set the audio unit callback.
    // 4) Initialize everything.
    // 5) Profit...

    AudioComponentDescription outputDesc = {0};
    outputDesc.componentType = kAudioUnitType_Output;
    outputDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
    outputDesc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent outComp = AudioComponentFindNext(NULL, &outputDesc);
    if (outComp == NULL) {
        ERROR("Can't find default output audio component.");
        return;
    }

    AudioComponentInstanceNew(outComp, &_coreAudio->_outputUnit);

    AURenderCallbackStruct callback;
    callback.inputProc = _coreAudio->audioCallback;
    callback.inputProcRefCon = _coreAudio;

    AudioUnitSetProperty(_coreAudio->_outputUnit,
        kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Input,
        0,
        &callback,
        sizeof(callback));

    AudioUnitInitialize(_coreAudio->_outputUnit);
    AudioOutputUnitStart(_coreAudio->_outputUnit);
}

void CoreAudioThread::run()
{
    DEBUG("Starting CoreAudio thread...");

    if (!_coreAudio->_outputUnit)
        initAudioLayer();

    // Actual playback is here.
    while (running_) {

    }
}


// AudioLayer implementation.
CoreLayer::CoreLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardring())
    , _playbackBuff(0, audioFormat_)
    , _captureBuff(0, audioFormat_)
//    , is_playback_prepared_(false)
//    , is_capture_prepared_(false)
//    , is_playback_running_(false)
//    , is_capture_running_(false)
//    , is_playback_open_(false)
//    , is_capture_open_(false)
    , audioThread_(nullptr)
    , mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{}

CoreLayer::~CoreLayer()
{
    isStarted_ = false;
}

std::vector<std::string> CoreLayer::getCaptureDeviceList() const
{
    std::vector<std::string> ret;

    for (auto x : getDeviceList(true))
        ret.push_back(x.Name);

    return ret;
}

std::vector<std::string> CoreLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> ret;

    for (auto x : getDeviceList(false))
        ret.push_back(x.Name);

    return ret;
}

int CoreLayer::getAudioDeviceIndex(const std::string& name, DeviceType type) const
{

}

std::string CoreLayer::getAudioDeviceName(int index, DeviceType type) const
{

}

void CoreLayer::startStream()
{
    dcblocker_.reset();

//    if (is_playback_running_ and is_capture_running_)
//        return;



    if (_audioThread == NULL) {
        _audioThread = new CoreAudioThread(this);
        _audioThread->start();
    } else if (!_audioThread->isRunning()) {
        _audioThread->start();
    }
}

void CoreLayer::stopStream()
{
    DEBUG("Stopping audio stream.");

    isStarted_ = false;

    AudioOutputUnitStop(outputUnit_);
    AudioUnitUninitialize(outputUnit_);
    AudioComponentInstanceDispose(outputUnit_);

    /* Flush the ring buffers */
    flushUrgent();
    flushMain();
}

void CoreLayer::initAudioFormat()
{
    AudioStreamBasicDescription info;
    UInt32 size = sizeof(info);
    AudioUnitGetProperty(_outputUnit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            0,
            &info,
            &size);
    DEBUG("Soundcard reports: %dKHz, %d channels.", (unsigned int)info.mSampleRate, (unsigned int)info.mChannelsPerFrame);

    std::cout << info.mChannelsPerFrame << std::endl;

    audioFormat_ = {(unsigned int)info.mSampleRate, (unsigned int)info.mChannelsPerFrame};
    hardwareFormatAvailable(audioFormat_);

    DEBUG("audioFormat_ set to: %dKHz, %d channels.", audioFormat_.sample_rate, audioFormat_.nb_channels);
}


OSStatus CoreLayer::audioCallback(void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    static_cast<CoreLayer*>(inRefCon)->write(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

void CoreLayer::write(AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    unsigned framesToGet = urgentRingBuffer_.availableForGet(RingBufferPool::DEFAULT_ID);

    if (framesToGet <= 0) {
        //WARN("Not enough samples to play audio.");
        for (int i = 0; i < inNumberFrames; ++i) {
            Float32* outDataL = (Float32*)ioData->mBuffers[0].mData;
            outDataL[i] = 0.0;
            Float32* outDataR = (Float32*)ioData->mBuffers[1].mData;
            outDataR[i] = 0.0;
        }
        return;
    }

    size_t totSample = std::min(inNumberFrames, framesToGet);

    _playbackBuff.setFormat(audioFormat_);
    _playbackBuff.resize(totSample);
    urgentRingBuffer_.get(_playbackBuff, RingBufferPool::DEFAULT_ID);

    _playbackBuff.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

    // TODO: Use correct number of channels
    for (int i = 0; i < audioFormat_.nb_channels; ++i)
        _playbackBuff.channelToFloat((Float32*)ioData->mBuffers[i].mData, 0); // Write

    Manager::instance().getRingBufferPool().discard(totSample, RingBufferPool::DEFAULT_ID);


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

    int nDevices = propsize / sizeof(AudioDeviceID);
    AudioDeviceID *devids = new AudioDeviceID[nDevices];

    verify_noerr(AudioObjectGetPropertyData(kAudioObjectSystemObject,
                        &theAddress,
                        0,
                        NULL,
                        &propsize,
                        devids));

    for (int i = 0; i < nDevices; ++i) {
        AudioDevice dev(devids[i], getCapture);
        if (dev.Channels > 0) { // Channels < 0 if inactive.
            ret.push_back(dev);
        }
    }
    delete[] devids;

    return ret;
}

}
