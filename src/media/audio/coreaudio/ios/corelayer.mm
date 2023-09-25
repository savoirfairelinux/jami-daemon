/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include <AVFoundation/AVAudioSession.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace jami {
dispatch_queue_t audioConfigurationQueueIOS() {
    static dispatch_once_t queueCreationGuard;
    static dispatch_queue_t queue;
    dispatch_once(&queueCreationGuard, ^{
        queue = dispatch_queue_create("audioConfigurationQueueIOS", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

enum AVSampleFormat
getFormatFromStreamDescription(const AudioStreamBasicDescription& descr) {
    if(descr.mFormatID == kAudioFormatLinearPCM) {
        BOOL isPlanar = descr.mFormatFlags & kAudioFormatFlagIsNonInterleaved;
        if(descr.mBitsPerChannel == 16) {
            if(descr.mFormatFlags & kAudioFormatFlagIsSignedInteger) {
                return isPlanar ? AV_SAMPLE_FMT_S16P : AV_SAMPLE_FMT_S16;
            }
        }
        else if(descr.mBitsPerChannel == 32) {
            if(descr.mFormatFlags & kAudioFormatFlagIsFloat) {
                return isPlanar ? AV_SAMPLE_FMT_FLTP : AV_SAMPLE_FMT_FLT;
            }
            else if(descr.mFormatFlags & kAudioFormatFlagIsSignedInteger) {
                return isPlanar ? AV_SAMPLE_FMT_S32P : AV_SAMPLE_FMT_S32;
            }
        }
    }
    NSLog(@"Unsupported core audio format");
    return AV_SAMPLE_FMT_NONE;
}

AudioFormat
audioFormatFromDescription(const AudioStreamBasicDescription& descr) {
    return AudioFormat {static_cast<unsigned int>(descr.mSampleRate),
                        static_cast<unsigned int>(descr.mChannelsPerFrame),
                        getFormatFromStreamDescription(descr)};
}

// AudioLayer implementation.
CoreLayer::CoreLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardRingtone())
{
     audioConfigurationQueue = dispatch_queue_create("com.savoirfairelinux.audioConfigurationQueueIOS", DISPATCH_QUEUE_SERIAL);
}

CoreLayer::~CoreLayer()
{
    dispatch_sync(audioConfigurationQueueIOS(), ^{
        if (status_ != Status::Started)
            return;
        destroyAudioLayer();
        flushUrgent();
        flushMain();
    });
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
CoreLayer::getAudioDeviceIndex(const std::string& name, AudioDeviceType type) const
{
    (void) name;
    (void) index;
    (void) type;
    return 0;
}

std::string
CoreLayer::getAudioDeviceName(int index, AudioDeviceType type) const
{
    (void) index;
    (void) type;
    return "";
}

bool
CoreLayer::initAudioLayerIO(AudioDeviceType stream)
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
        return false;
    }

    checkErr(AudioComponentInstanceNew(comp, &ioUnit_));

    bool setUpInput = stream == AudioDeviceType::ALL || stream == AudioDeviceType::CAPTURE;
    NSError* error = nil;
    AVAudioSessionCategory audioCategory = setUpInput ? AVAudioSessionCategoryPlayAndRecord : AVAudioSessionCategoryPlayback;
    AVAudioSessionMode mode = setUpInput ? AVAudioSessionModeVoiceChat : AVAudioSessionModeMoviePlayback;
    AVAudioSessionCategoryOptions options = setUpInput ? AVAudioSessionCategoryOptionAllowBluetooth : AVAudioSessionCategoryOptionMixWithOthers;
    [[AVAudioSession sharedInstance] setCategory:audioCategory mode: mode options:options error:&error];
    if (error) {
        NSLog(@"Initializing audio session failed, %@",[error localizedDescription]);
        return false;
    }
    [[AVAudioSession sharedInstance] setActive: true withOptions: AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error: &error];
    if (error) {
        NSLog(@"Set active audio session failed, %@",[error localizedDescription]);
        return false;
    }
    auto playBackDeviceList = getPlaybackDeviceList();
    JAMI_DBG("Setting playback device: %s", playBackDeviceList[indexOut_].c_str());
    switch(indexOut_) {
        case 0:
            [[AVAudioSession sharedInstance] overrideOutputAudioPort:AVAudioSessionPortOverrideSpeaker error:nil];
            break;
        case 1:
        case 2:
            break;
        case 3:
            [[AVAudioSession sharedInstance] overrideOutputAudioPort:AVAudioSessionPortOverrideNone error:nil];
            break;
        default:
            break;
    }
    setupOutputBus();
    if (setUpInput) {
        setupInputBus();
    }
    bindCallbacks();
    return true;
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
                             static_cast<unsigned int>(outputASBD.mChannelsPerFrame),
                             getFormatFromStreamDescription(outputASBD)});
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

    AVAudioSession *session = [AVAudioSession sharedInstance];
    // Replace AudioSessionGetProperty with AVAudioSession
    Float64 inSampleRate = session.sampleRate;
    Float32 bufferDuration = session.IOBufferDuration;

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
                         static_cast<unsigned int>(inputASBD.mChannelsPerFrame),
                         getFormatFromStreamDescription(inputASBD)};
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
CoreLayer::startStream(AudioDeviceType stream)
{
    dispatch_async(audioConfigurationQueueIOS(), ^{
        JAMI_DBG("iOS CoreLayer - Start Stream");
        auto currentCategory =  [[AVAudioSession sharedInstance] category];

        bool updateStream = currentCategory == AVAudioSessionCategoryPlayback && (stream == AudioDeviceType::CAPTURE || stream == AudioDeviceType::ALL);
        if (status_ == Status::Started) {
            if (updateStream)
                destroyAudioLayer();
            else
                return;
        }
        status_ = Status::Started;

        if (!initAudioLayerIO(stream) || AudioUnitInitialize(ioUnit_) || AudioOutputUnitStart(ioUnit_)) {
            destroyAudioLayer();
            status_ = Status::Idle;
        }
    });
}

void
CoreLayer::destroyAudioLayer()
{
    JAMI_DBG("iOS CoreLayer - destroy Audio layer");
    AudioOutputUnitStop(ioUnit_);
    AudioUnitUninitialize(ioUnit_);
    AudioComponentInstanceDispose(ioUnit_);
    [[AVAudioSession sharedInstance] setActive: false withOptions: AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error: nil];
}

void
CoreLayer::stopStream(AudioDeviceType stream)
{
    dispatch_async(audioConfigurationQueueIOS(), ^{
        JAMI_DBG("iOS CoreLayer - Stop Stream");
        auto currentCategory =  [[AVAudioSession sharedInstance] category];
        bool keepCurrentStream = currentCategory == AVAudioSessionCategoryPlayAndRecord && (stream == AudioDeviceType::PLAYBACK);
        if (status_ != Status::Started || keepCurrentStream)
            return;
        status_ = Status::Idle;
        destroyAudioLayer();
    });
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
        for (unsigned i = 0; i < frame.ch_layout.nb_channels; ++i) {
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

    auto format = audioInputFormat_;
    format.sampleFormat = AV_SAMPLE_FMT_FLTP;
    auto inBuff = std::make_shared<AudioFrame>(format, inNumberFrames);
    AudioBufferList buffer;
    UInt32 bufferSize = inNumberFrames * sizeof(Float32);
    buffer.mNumberBuffers = inChannelsPerFrame_;
    for (UInt32 i = 0; i < buffer.mNumberBuffers; ++i) {
        buffer.mBuffers[i].mNumberChannels = 1;
        buffer.mBuffers[i].mDataByteSize = bufferSize;
        buffer.mBuffers[i].mData = inBuff->pointer()->extended_data[i];
    }

    // Write the mic samples in our buffer
    checkErr(AudioUnitRender(ioUnit_,
            ioActionFlags,
            inTimeStamp,
            inBusNumber,
            inNumberFrames,
            &buffer));

    if (isCaptureMuted_) {
        libav_utils::fillWithSilence(inBuff->pointer());
    }
    putRecorded(std::move(inBuff));
}

void CoreLayer::updatePreference(AudioPreference &preference, int index, AudioDeviceType type)
{
    switch (type) {
        case AudioDeviceType::ALL:
        case AudioDeviceType::PLAYBACK:
            preference.setAlsaCardout(index);
            break;

        case AudioDeviceType::CAPTURE:
            preference.setAlsaCardin(index);
            break;

        case AudioDeviceType::RINGTONE:
            preference.setAlsaCardRingtone(index);
            break;

        default:
            break;
    }
}

} // namespace jami

#pragma GCC diagnostic pop
