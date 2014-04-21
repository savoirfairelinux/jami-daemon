/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "opensllayer.h"

#include "client/configurationmanager.h"

#include "manager.h"
#include "mainbuffer.h"
#include "audio/dcblocker.h"
#include "logger.h"
#include "array_size.h"

#include <thread>
#include <chrono>
#include <cstdio>
#include <cassert>
#include <unistd.h>

const int OpenSLLayer::NB_BUFFER_PLAYBACK_QUEUE = ANDROID_BUFFER_QUEUE_LENGTH;
const int OpenSLLayer::NB_BUFFER_CAPTURE_QUEUE = ANDROID_BUFFER_QUEUE_LENGTH;

// Constructor
OpenSLLayer::OpenSLLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(0)
    , indexOut_(0)
    , indexRing_(0)
    , audioThread_(nullptr)
    , engineObject_(nullptr)
    , engineInterface_(nullptr)
    , outputMixer_(nullptr)
    , playerObject_(nullptr)
    , recorderObject_(nullptr)
    , playerInterface_(nullptr)
    , recorderInterface_(nullptr)
    , playbackBufferQueue_(nullptr)
    , recorderBufferQueue_(nullptr)
    , playbackBufferIndex_(0)
    , recordBufferIndex_(0)
    , hardwareFormat_(AudioFormat::MONO)
    , hardwareBuffSize_(BUFFER_SIZE)
    , playbackBufferStack_(ANDROID_BUFFER_QUEUE_LENGTH, AudioBuffer(hardwareBuffSize_, AudioFormat::MONO))
    , recordBufferStack_(ANDROID_BUFFER_QUEUE_LENGTH, AudioBuffer(hardwareBuffSize_, AudioFormat::MONO))
{
}

// Destructor
OpenSLLayer::~OpenSLLayer()
{
    isStarted_ = false;

    /* Then close the audio devices */
    stopAudioPlayback();
    stopAudioCapture();
}

void
OpenSLLayer::init()
{
    initAudioEngine();
    initAudioPlayback();
    initAudioCapture();

    flushMain();
    flushUrgent();
}

void
OpenSLLayer::startStream()
{
    dcblocker_.reset();

    if (isStarted_)
        return;

    DEBUG("Start OpenSL audio layer");

    std::vector<int32_t> hw_infos = Manager::instance().getClient()->getConfigurationManager()->getHardwareAudioFormat();
    hardwareFormat_ = AudioFormat(hw_infos[0], 1);  // Mono on Android
    hardwareBuffSize_ = hw_infos[1];

    for(auto& buf : playbackBufferStack_)
        buf.resize(hardwareBuffSize_);
    for(auto& buf : recordBufferStack_)
        buf.resize(hardwareBuffSize_);

    hardwareFormatAvailable(hardwareFormat_);

    std::thread launcher([this](){
        init();
        startAudioPlayback();
        startAudioCapture();
        isStarted_ = true;
    });
    launcher.detach();
}

void
OpenSLLayer::stopStream()
{
    if (not isStarted_)
        return;

    DEBUG("Stop OpenSL audio layer");

    stopAudioPlayback();
    stopAudioCapture();

    isStarted_ = false;

    flushMain();
    flushUrgent();
}

void
OpenSLLayer::initAudioEngine()
{
    SLresult result;

    DEBUG("Create Audio Engine\n");
    result = slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Realize Audio Engine\n");
    result = (*engineObject_)->Realize(engineObject_, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Create Audio Engine Interface\n");
    result = (*engineObject_)->GetInterface(engineObject_, SL_IID_ENGINE, &engineInterface_);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Create Output Mixer\n");
    result = (*engineInterface_)->CreateOutputMix(engineInterface_, &outputMixer_, 0, nullptr, nullptr);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Realize Output Mixer\n");
    result = (*outputMixer_)->Realize(outputMixer_, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Audio Engine Initialization Done\n");
}

void
OpenSLLayer::shutdownAudioEngine()
{

    // destroy buffer queue audio player object, and invalidate all associated interfaces
    DEBUG("Shutdown audio player\n");

    if (playerObject_ != nullptr) {
        (*playerObject_)->Destroy(playerObject_);
        playerObject_ = nullptr;
        playerInterface_ = nullptr;
        playbackBufferQueue_ = nullptr;
    }

    // destroy output mix object, and invalidate all associated interfaces
    DEBUG("Shutdown audio mixer\n");

    if (outputMixer_ != nullptr) {
        (*outputMixer_)->Destroy(outputMixer_);
        outputMixer_ = nullptr;
    }

    if (recorderObject_ != nullptr) {
        (*recorderObject_)->Destroy(recorderObject_);
        recorderObject_ = nullptr;
        recorderInterface_ = nullptr;
        recorderBufferQueue_ = nullptr;
    }

    // destroy engine object, and invalidate all associated interfaces
    DEBUG("Shutdown audio engine\n");
    if (engineObject_ != nullptr) {
        (*engineObject_)->Destroy(engineObject_);
        engineObject_ = nullptr;
        engineInterface_ = nullptr;
    }
}

void
OpenSLLayer::initAudioPlayback()
{
    assert(nullptr != engineObject_);
    assert(nullptr != engineInterface_);
    assert(nullptr != outputMixer_);

    SLresult result;

    // Initialize the location of the buffer queue
    DEBUG("Create playback queue\n");
    SLDataLocator_AndroidSimpleBufferQueue bufferLocation = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                            NB_BUFFER_PLAYBACK_QUEUE
                                                            };

    // Initnialize the audio format for this queue
    DEBUG("Setting audio format\n");
    SLDataFormat_PCM audioFormat = {SL_DATAFORMAT_PCM,
                                    1,
                                    audioFormat_.sample_rate * 1000,
                                    SL_PCMSAMPLEFORMAT_FIXED_16,
                                    SL_PCMSAMPLEFORMAT_FIXED_16,
                                    SL_SPEAKER_FRONT_CENTER,
                                    SL_BYTEORDER_LITTLEENDIAN
                                   };

    // Create the audio source
    DEBUG("Set Audio Sources\n");
    SLDataSource audioSource = {&bufferLocation, &audioFormat};

    DEBUG("Get Output Mixer interface\n");
    result = (*outputMixer_)->GetInterface(outputMixer_, SL_IID_OUTPUTMIX, &outputMixInterface_);
    CheckErr(result);

    // Cofiguration fo the audio sink as an output mixer
    DEBUG("Set output mixer location\n");
    SLDataLocator_OutputMix mixerLocation = {SL_DATALOCATOR_OUTPUTMIX, outputMixer_};
    SLDataSink audioSink = {&mixerLocation, nullptr};

    const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                 SL_IID_VOLUME,
                                 SL_IID_ANDROIDCONFIGURATION,
                                 SL_IID_PLAY};
    const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    const unsigned nbInterface = ARRAYSIZE(ids);

    // create audio player
    DEBUG("Create audio player\n");
    result = (*engineInterface_)->CreateAudioPlayer(engineInterface_, &playerObject_, &audioSource, &audioSink, nbInterface, ids, req);
    assert(SL_RESULT_SUCCESS == result);

    SLAndroidConfigurationItf playerConfig;
    SLint32 streamType = SL_ANDROID_STREAM_VOICE;


    result = (*playerObject_)->GetInterface(playerObject_,
                                            SL_IID_ANDROIDCONFIGURATION,
                                            &playerConfig);

    if (result == SL_RESULT_SUCCESS && playerConfig) {
        result = (*playerConfig)->SetConfiguration(
                     playerConfig, SL_ANDROID_KEY_STREAM_TYPE,
                     &streamType, sizeof(SLint32));
    }

    DEBUG("Realize audio player\n");
    result = (*playerObject_)->Realize(playerObject_, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    if (result != SL_RESULT_SUCCESS) {
        ERROR("Unable to set android player configuration");
    }

    // create audio interface
    DEBUG("Create audio player interface\n");
    result = (*playerObject_)->GetInterface(playerObject_, SL_IID_PLAY, &playerInterface_);
    assert(SL_RESULT_SUCCESS == result);

    // create the buffer queue interface
    DEBUG("Create buffer queue interface\n");
    result = (*playerObject_)->GetInterface(playerObject_, SL_IID_BUFFERQUEUE, &playbackBufferQueue_);
    assert(SL_RESULT_SUCCESS == result);

    // register the buffer queue on the buffer object
    DEBUG("Register audio callback\n");
    result = (*playbackBufferQueue_)->RegisterCallback(playbackBufferQueue_, audioPlaybackCallback, this);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Audio Playback Initialization Done\n");
}

void
OpenSLLayer::initAudioCapture()
{
    SLresult result;

    // configure audio source
    DEBUG("Configure audio source\n");
    SLDataLocator_IODevice deviceLocator = {SL_DATALOCATOR_IODEVICE,
                                            SL_IODEVICE_AUDIOINPUT,
                                            SL_DEFAULTDEVICEID_AUDIOINPUT,
                                            nullptr
                                           };

    SLDataSource audioSource = {&deviceLocator,
                                nullptr
                               };

    // configure audio sink
    DEBUG("Configure audio sink\n");

    SLDataLocator_AndroidSimpleBufferQueue bufferLocator = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                           NB_BUFFER_CAPTURE_QUEUE
                                                           };

    DEBUG("Capture-> Sampling Rate: %d", audioFormat_.sample_rate);
    DEBUG("Capture-> getInternalSamplingRate: %d", Manager::instance().getMainBuffer().getInternalSamplingRate());
    SLDataFormat_PCM audioFormat = {SL_DATAFORMAT_PCM, 1,
                                    audioFormat_.sample_rate * 1000,
                                    SL_PCMSAMPLEFORMAT_FIXED_16,
                                    SL_PCMSAMPLEFORMAT_FIXED_16,
                                    SL_SPEAKER_FRONT_CENTER,
                                    SL_BYTEORDER_LITTLEENDIAN
                                   };

    SLDataSink audioSink = {&bufferLocator,
                            &audioFormat
                           };

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    DEBUG("Create audio recorder\n");
    const SLInterfaceID id[2] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                SL_IID_ANDROIDCONFIGURATION};
    const SLboolean req[2] ={SL_BOOLEAN_TRUE,
                             SL_BOOLEAN_TRUE};
    SLAndroidConfigurationItf recorderConfig;

    if (engineInterface_ != nullptr) {
        result = (*engineInterface_)->CreateAudioRecorder(engineInterface_,
                 &recorderObject_, &audioSource, &audioSink, 2, id, req);
    }

    if (SL_RESULT_SUCCESS != result) {
        DEBUG("Error: could not create audio recorder");
        return;
    }

    /* Set Android configuration */
    result = (*recorderObject_)->GetInterface(recorderObject_,
                                                SL_IID_ANDROIDCONFIGURATION,
                                                &recorderConfig);
    if (result == SL_RESULT_SUCCESS) {
       SLint32 streamType = SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;
       result = (*recorderConfig)->SetConfiguration(
                recorderConfig, SL_ANDROID_KEY_RECORDING_PRESET,
                &streamType, sizeof(SLint32));
    }

    if (result != SL_RESULT_SUCCESS) {
        DEBUG("Warning: Unable to set android recorder configuration");
        return;
    }

    // realize the audio recorder
    DEBUG("Realize the audio recorder\n");
    result = (*recorderObject_)->Realize(recorderObject_, SL_BOOLEAN_FALSE);

    if (SL_RESULT_SUCCESS != result) {
        DEBUG("Error: could not realize audio recorder");
        return;
    }

    // get the record interface
    DEBUG("Create the record interface\n");
    result = (*recorderObject_)->GetInterface(recorderObject_, SL_IID_RECORD, &recorderInterface_);
    assert(SL_RESULT_SUCCESS == result);

    // get the buffer queue interface
    DEBUG("Create the buffer queue interface\n");
    result = (*recorderObject_)->GetInterface(recorderObject_, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
             &recorderBufferQueue_);
    assert(SL_RESULT_SUCCESS == result);

    // register callback on the buffer queue
    DEBUG("Register the audio capture callback\n");
    result = (*recorderBufferQueue_)->RegisterCallback(recorderBufferQueue_, audioCaptureCallback, this);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Audio capture initialized\n");
}


void
OpenSLLayer::startAudioPlayback()
{
    assert(nullptr != playbackBufferQueue_);

    DEBUG("Start audio playback\n");

    SLresult result;

    for (int i = 0; i < NB_BUFFER_PLAYBACK_QUEUE; i++) {
        AudioBuffer &buffer = getNextPlaybackBuffer();
        incrementPlaybackIndex();

        buffer.reset();

        result = (*playbackBufferQueue_)->Enqueue(playbackBufferQueue_, buffer.getChannel(0)->data(), buffer.frames());

        if (SL_RESULT_SUCCESS != result) {
            DEBUG("Error could not enqueue initial buffers\n");
        }
    }

    result = (*playerInterface_)->SetPlayState(playerInterface_, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Audio playback started\n");
}

void
OpenSLLayer::startAudioCapture()
{
    assert(nullptr != playbackBufferQueue_);

    DEBUG("Start audio capture\n");

    SLresult result;


    // in case already recording, stop recording and clear buffer queue
    if (recorderInterface_ != nullptr) {
        result = (*recorderInterface_)->SetRecordState(recorderInterface_, SL_RECORDSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);
    }

    DEBUG("Clearing recorderBufferQueue\n");
    result = (*recorderBufferQueue_)->Clear(recorderBufferQueue_);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("getting next record buffer\n");
    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we enqueue at least 2 empty buffers to start things off)
    AudioBuffer &buffer = getNextRecordBuffer();
    incrementRecordIndex();

    buffer.reset();

    DEBUG("Enqueue record buffer\n");
    result = (*recorderBufferQueue_)->Enqueue(recorderBufferQueue_, buffer.getChannel(0)->data(), buffer.frames());

    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    if (SL_RESULT_SUCCESS != result) {
        DEBUG("Error could not enqueue buffers in audio capture\n");
        return;
    }

    // start recording
    result = (*recorderInterface_)->SetRecordState(recorderInterface_, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Audio capture started\n");
}

void
OpenSLLayer::stopAudioPlayback()
{
    DEBUG("Stop audio playback\n");

    if (playerInterface_ != nullptr) {
        SLresult result;
        result = (*playerInterface_)->SetPlayState(playerInterface_, SL_PLAYSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);
    }

    DEBUG("Audio playback stopped\n");
}

void
OpenSLLayer::stopAudioCapture()
{
    DEBUG("Stop audio capture\n");

    if (recorderInterface_ != nullptr) {
        SLresult result;
        result = (*recorderInterface_)->SetRecordState(recorderInterface_, SL_RECORDSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);
    }

    DEBUG("Audio capture stopped\n");

}

std::vector<std::string>
OpenSLLayer::getCaptureDeviceList() const
{
    std::vector<std::string> captureDeviceList;



// Although OpenSL ES specification allows enumerating
// available output (and also input) devices, NDK implementation is not mature enough to
// obtain or select proper one (SLAudioIODeviceCapabilitiesItf, the official interface
// to obtain such an information)-> SL_FEATURE_UNSUPPORTED

    SLuint32 InputDeviceIDs[MAX_NUMBER_INPUT_DEVICES];
    SLint32 numInputs = 0;
    SLboolean mic_available = SL_BOOLEAN_FALSE;
    SLuint32 mic_deviceID = 0;

    SLresult res;

    initAudioEngine();
    initAudioPlayback();
    initAudioCapture();


    // Get the Audio IO DEVICE CAPABILITIES interface, implicit
    DEBUG("Get the Audio IO DEVICE CAPABILITIES interface, implicit");

    res = (*engineObject_)->GetInterface(engineObject_, SL_IID_AUDIOIODEVICECAPABILITIES, (void*)&AudioIODeviceCapabilitiesItf);
    CheckErr(res);

    DEBUG("Get the Audio IO DEVICE CAPABILITIES interface, implicit");
    numInputs = MAX_NUMBER_INPUT_DEVICES;

    res = (*AudioIODeviceCapabilitiesItf)->GetAvailableAudioInputs(AudioIODeviceCapabilitiesItf, &numInputs, InputDeviceIDs);
    CheckErr(res);

    // Search for either earpiece microphone or headset microphone input
    // device - with a preference for the latter
    for (int i = 0; i < numInputs; i++) {
        res = (*AudioIODeviceCapabilitiesItf)->QueryAudioInputCapabilities(AudioIODeviceCapabilitiesItf,
                                                                           InputDeviceIDs[i],
                                                                           &AudioInputDescriptor);
        CheckErr(res);

        if (AudioInputDescriptor.deviceConnection == SL_DEVCONNECTION_ATTACHED_WIRED and
            AudioInputDescriptor.deviceScope == SL_DEVSCOPE_USER and
            AudioInputDescriptor.deviceLocation == SL_DEVLOCATION_HEADSET) {
            DEBUG("SL_DEVCONNECTION_ATTACHED_WIRED : mic_deviceID: %d", InputDeviceIDs[i] );
            mic_deviceID = InputDeviceIDs[i];
            mic_available = SL_BOOLEAN_TRUE;
            break;
        } else if (AudioInputDescriptor.deviceConnection == SL_DEVCONNECTION_INTEGRATED and
                   AudioInputDescriptor.deviceScope == SL_DEVSCOPE_USER and
                   AudioInputDescriptor.deviceLocation == SL_DEVLOCATION_HANDSET) {
            DEBUG("SL_DEVCONNECTION_INTEGRATED : mic_deviceID: %d", InputDeviceIDs[i] );
            mic_deviceID = InputDeviceIDs[i];
            mic_available = SL_BOOLEAN_TRUE;
            break;
        }
    }

    if (!mic_available) {
        // Appropriate error message here
        ERROR("No mic available quitting");
        exit(1);
    }


    return captureDeviceList;
}

/* Checks for error. If any errors exit the application! */
void
OpenSLLayer::CheckErr( SLresult res )
{
    if (res != SL_RESULT_SUCCESS) {
        // Debug printing to be placed here
        exit(1);
    }
}

std::vector<std::string>
OpenSLLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> playbackDeviceList;

    return playbackDeviceList;
}

void
OpenSLLayer::audioPlaybackCallback(SLAndroidSimpleBufferQueueItf queue, void *context)
{
    assert(nullptr != context);
    //auto start = std::chrono::high_resolution_clock::now();
    static_cast<OpenSLLayer*>(context)->playback(queue);
    //auto end = std::chrono::high_resolution_clock::now();
    //auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    //DEBUG("Took %d us\n", elapsed/1000);
}

void
OpenSLLayer::playback(SLAndroidSimpleBufferQueueItf queue)
{
    assert(nullptr != queue);
    notifyIncomingCall();

    AudioBuffer &buffer = getNextPlaybackBuffer();
    size_t urgentSamplesToGet = urgentRingBuffer_.availableForGet(MainBuffer::DEFAULT_ID);

    bufferIsFilled_ = false;
    if (urgentSamplesToGet > 0) {
        bufferIsFilled_ = audioPlaybackFillWithUrgent(buffer, std::min(urgentSamplesToGet, hardwareBuffSize_));
    } else {
        auto& main_buffer = Manager::instance().getMainBuffer();
        buffer.resize(hardwareBuffSize_);
        size_t samplesToGet = audioPlaybackFillWithVoice(buffer);
        if (samplesToGet == 0) {
            bufferIsFilled_ = audioPlaybackFillWithToneOrRingtone(buffer);
        } else {
            bufferIsFilled_ = true;
        }
    }

    if (bufferIsFilled_) {
        SLresult result = (*queue)->Enqueue(queue, buffer.getChannel(0)->data(), buffer.frames()*sizeof(SFLAudioSample));
        if (SL_RESULT_SUCCESS != result) {
            DEBUG("Error could not enqueue buffers in playback callback\n");
        }
        incrementPlaybackIndex();
    } else {
        DEBUG("Error buffer not filled in audio playback\n");
    }
}

void
OpenSLLayer::audioCaptureCallback(SLAndroidSimpleBufferQueueItf queue, void *context)
{
    assert(nullptr != context);
    static_cast<OpenSLLayer*>(context)->capture(queue);
}

void
OpenSLLayer::capture(SLAndroidSimpleBufferQueueItf queue)
{
    assert(nullptr != queue);

    AudioBuffer &old_buffer = getNextRecordBuffer();
    incrementRecordIndex();
    AudioBuffer &buffer = getNextRecordBuffer();

    SLresult result;
    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we enqueue at least 2 empty buffers to start things off)
    result = (*recorderBufferQueue_)->Enqueue(recorderBufferQueue_, buffer.getChannel(0)->data(), buffer.frames()*sizeof(SFLAudioSample));

    audioCaptureFillBuffer(old_buffer);

    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    assert(SL_RESULT_SUCCESS == result);
}



void
OpenSLLayer::updatePreference(AudioPreference &preference, int index, DeviceType type)
{
#ifdef OUTSIDE_TESTING

    switch (type) {
        case Device::PLAYBACK:
            break;

        case Device::CAPTURE:
            break;

        case Device::RINGTONE:
            break;
    }

#endif
}

void OpenSLLayer::audioCaptureFillBuffer(AudioBuffer &buffer)
{
    MainBuffer &mbuffer = Manager::instance().getMainBuffer();

    //const unsigned mainBufferSampleRate = mbuffer.getInternalSamplingRate();
    const AudioFormat mainBufferFormat = mbuffer.getInternalAudioFormat();
    const bool resample = mainBufferFormat.sample_rate != audioFormat_.sample_rate;

    buffer.applyGain(isCaptureMuted_ ? 0.0 : captureGain_);

    if (resample) {
        int outSamples = buffer.frames() * (static_cast<double>(audioFormat_.sample_rate) / mainBufferFormat.sample_rate);
        AudioBuffer out(outSamples, mainBufferFormat);
        resampler_.resample(buffer, out);
        dcblocker_.process(out);
        mbuffer.putData(out, MainBuffer::DEFAULT_ID);
    } else {
        dcblocker_.process(buffer);
        mbuffer.putData(buffer, MainBuffer::DEFAULT_ID);
    }
}

bool OpenSLLayer::audioPlaybackFillWithToneOrRingtone(AudioBuffer &buffer)
{
    buffer.resize(hardwareBuffSize_);
    AudioLoop *tone = Manager::instance().getTelephoneTone();
    AudioLoop *file_tone = Manager::instance().getTelephoneFile();

    // In case of a dtmf, the pointers will be set to nullptr once the dtmf length is
    // reached. For this reason we need to fill audio buffer with zeros if pointer is nullptr
    if (tone) {
        tone->getNext(buffer, playbackGain_);
    } else if (file_tone) {
        file_tone->getNext(buffer, playbackGain_);
    } else {
        buffer.reset();
    }

    return true;
}

bool OpenSLLayer::audioPlaybackFillWithUrgent(AudioBuffer &buffer, size_t samplesToGet)
{
    // Urgent data (dtmf, incoming call signal) come first.
    samplesToGet = std::min(samplesToGet, hardwareBuffSize_);
    buffer.resize(samplesToGet);
    urgentRingBuffer_.get(buffer, MainBuffer::DEFAULT_ID);
    buffer.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

    // Consume the regular one as well (same amount of samples)
    Manager::instance().getMainBuffer().discard(samplesToGet, MainBuffer::DEFAULT_ID);

    return true;
}

size_t OpenSLLayer::audioPlaybackFillWithVoice(AudioBuffer &buffer)
{
    MainBuffer &mainBuffer = Manager::instance().getMainBuffer();
    size_t got = mainBuffer.getAvailableData(buffer, MainBuffer::DEFAULT_ID);
    buffer.resize(got);
    buffer.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
    if (audioFormat_.sample_rate != mainBuffer.getInternalSamplingRate()) {
        DEBUG("OpenSLLayer::audioPlaybackFillWithVoice sample_rate != mainBuffer.getInternalSamplingRate() \n");
        AudioBuffer out(buffer, false);
        out.setSampleRate(audioFormat_.sample_rate);
        resampler_.resample(buffer, out);
        buffer = out;
    }
    return buffer.size();
}

void dumpAvailableEngineInterfaces()
{
    SLresult result;
    DEBUG("Engine Interfaces\n");
    SLuint32 numSupportedInterfaces;
    result = slQueryNumSupportedEngineInterfaces(&numSupportedInterfaces);
    assert(SL_RESULT_SUCCESS == result);
    result = slQueryNumSupportedEngineInterfaces(NULL);
    assert(SL_RESULT_PARAMETER_INVALID == result);

    DEBUG("Engine number of supported interfaces %lu\n", numSupportedInterfaces);
    for(SLuint32 i=0; i< numSupportedInterfaces; i++){
        SLInterfaceID  pInterfaceId;
        slQuerySupportedEngineInterfaces(i, &pInterfaceId);
        const char* nm = "unknown iid";

        if (pInterfaceId==SL_IID_NULL) nm="null";
        else if (pInterfaceId==SL_IID_OBJECT) nm="object";
        else if (pInterfaceId==SL_IID_AUDIOIODEVICECAPABILITIES) nm="audiodevicecapabilities";
        else if (pInterfaceId==SL_IID_LED) nm="led";
        else if (pInterfaceId==SL_IID_VIBRA) nm="vibra";
        else if (pInterfaceId==SL_IID_METADATAEXTRACTION) nm="metadataextraction";
        else if (pInterfaceId==SL_IID_METADATATRAVERSAL) nm="metadatatraversal";
        else if (pInterfaceId==SL_IID_DYNAMICSOURCE) nm="dynamicsource";
        else if (pInterfaceId==SL_IID_OUTPUTMIX) nm="outputmix";
        else if (pInterfaceId==SL_IID_PLAY) nm="play";
        else if (pInterfaceId==SL_IID_PREFETCHSTATUS) nm="prefetchstatus";
        else if (pInterfaceId==SL_IID_PLAYBACKRATE) nm="playbackrate";
        else if (pInterfaceId==SL_IID_SEEK) nm="seek";
        else if (pInterfaceId==SL_IID_RECORD) nm="record";
        else if (pInterfaceId==SL_IID_EQUALIZER) nm="equalizer";
        else if (pInterfaceId==SL_IID_VOLUME) nm="volume";
        else if (pInterfaceId==SL_IID_DEVICEVOLUME) nm="devicevolume";
        else if (pInterfaceId==SL_IID_BUFFERQUEUE) nm="bufferqueue";
        else if (pInterfaceId==SL_IID_PRESETREVERB) nm="presetreverb";
        else if (pInterfaceId==SL_IID_ENVIRONMENTALREVERB) nm="environmentalreverb";
        else if (pInterfaceId==SL_IID_EFFECTSEND) nm="effectsend";
        else if (pInterfaceId==SL_IID_3DGROUPING) nm="3dgrouping";
        else if (pInterfaceId==SL_IID_3DCOMMIT) nm="3dcommit";
        else if (pInterfaceId==SL_IID_3DLOCATION) nm="3dlocation";
        else if (pInterfaceId==SL_IID_3DDOPPLER) nm="3ddoppler";
        else if (pInterfaceId==SL_IID_3DSOURCE) nm="3dsource";
        else if (pInterfaceId==SL_IID_3DMACROSCOPIC) nm="3dmacroscopic";
        else if (pInterfaceId==SL_IID_MUTESOLO) nm="mutesolo";
        else if (pInterfaceId==SL_IID_DYNAMICINTERFACEMANAGEMENT) nm="dynamicinterfacemanagement";
        else if (pInterfaceId==SL_IID_MIDIMESSAGE) nm="midimessage";
        else if (pInterfaceId==SL_IID_MIDIMUTESOLO) nm="midimutesolo";
        else if (pInterfaceId==SL_IID_MIDITEMPO) nm="miditempo";
        else if (pInterfaceId==SL_IID_MIDITIME) nm="miditime";
        else if (pInterfaceId==SL_IID_AUDIODECODERCAPABILITIES) nm="audiodecodercapabilities";
        else if (pInterfaceId==SL_IID_AUDIOENCODERCAPABILITIES) nm="audioencodercapabilities";
        else if (pInterfaceId==SL_IID_AUDIOENCODER) nm="audioencoder";
        else if (pInterfaceId==SL_IID_BASSBOOST) nm="bassboost";
        else if (pInterfaceId==SL_IID_PITCH) nm="pitch";
        else if (pInterfaceId==SL_IID_RATEPITCH) nm="ratepitch";
        else if (pInterfaceId==SL_IID_VIRTUALIZER) nm="virtualizer";
        else if (pInterfaceId==SL_IID_VISUALIZATION) nm="visualization";
        else if (pInterfaceId==SL_IID_ENGINE) nm="engine";
        else if (pInterfaceId==SL_IID_ENGINECAPABILITIES) nm="enginecapabilities";
        else if (pInterfaceId==SL_IID_THREADSYNC) nm="theadsync";
        else if (pInterfaceId==SL_IID_ANDROIDEFFECT) nm="androideffect";
        else if (pInterfaceId==SL_IID_ANDROIDEFFECTSEND) nm="androideffectsend";
        else if (pInterfaceId==SL_IID_ANDROIDEFFECTCAPABILITIES) nm="androideffectcapabilities";
        else if (pInterfaceId==SL_IID_ANDROIDCONFIGURATION) nm="androidconfiguration";
        else if (pInterfaceId==SL_IID_ANDROIDSIMPLEBUFFERQUEUE) nm="simplebuferqueue";
        //else if (pInterfaceId==//SL_IID_ANDROIDBUFFERQUEUESOURCE) nm="bufferqueuesource";
        DEBUG("%s,",nm);
    }
}
