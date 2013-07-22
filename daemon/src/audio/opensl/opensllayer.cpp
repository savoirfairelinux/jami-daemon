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

#include <cstdio>
#include <cassert>

#include "logger.h"
#include "array_size.h"
#include "manager.h"
#include "mainbuffer.h"
#include "cc++/thread.h"

#include "opensllayer.h"

const int OpenSLLayer::NB_BUFFER_PLAYBACK_QUEUE = ANDROID_BUFFER_QUEUE_LENGTH;
const int OpenSLLayer::NB_BUFFER_CAPTURE_QUEUE = ANDROID_BUFFER_QUEUE_LENGTH;

static long sawPtr = 0;
static void generateSawTooth(short *buffer, int length)
{
    assert(NULL != buffer);
    assert(length > 0);

    unsigned int i;

    for (i = 0; i < length; ++i, ++sawPtr) {
        buffer[i] = 32768 - ((sawPtr % 10) * 6600);
    }
}

class OpenSLThread : public ost::Thread {
    public:
        OpenSLThread(OpenSLLayer *opensl);
        ~OpenSLThread();
        void initAudioLayer();
        virtual void run();

    private:
        NON_COPYABLE(OpenSLThread);
        OpenSLLayer* opensl_;
};

OpenSLThread::OpenSLThread(OpenSLLayer *opensl)
    : ost::Thread(), opensl_(opensl)
{}

OpenSLThread::~OpenSLThread()
{
    opensl_->shutdownAudioEngine();

    ost::Thread::terminate();
}

void OpenSLThread::initAudioLayer(void)
{
    opensl_->initAudioEngine();
    opensl_->initAudioPlayback();
    opensl_->initAudioCapture();
}

/**
 * Reimplementation of run()
 */
void OpenSLThread::run()
{
    initAudioLayer();

    opensl_->startAudioPlayback();
    opensl_->startAudioCapture();

    while (opensl_->isStarted_) {
        ost::Thread::sleep(20 /* ms */);
    }
}

// Constructor
OpenSLLayer::OpenSLLayer()
    : indexIn_(0)
    , indexOut_(0)
    , indexRing_(0)
    , audioThread_(0)
    , isStarted_(false)
    , engineObject_(0)
    , engineInterface_(0)
    , outputMixer_(0)
    , playerObject_(0)
    , recorderObject_(0)
    , playerInterface_(0)
    , recorderInterface_(0)
    , playbackBufferQueue_(0)
    , recorderBufferQueue_(0)
    , playbackBufferIndex_(0)
    , recordBufferIndex_(0)
    , playbackBufferStack_(ANDROID_BUFFER_QUEUE_LENGTH)
    , recordBufferStack_(ANDROID_BUFFER_QUEUE_LENGTH)
{
}

// Destructor
OpenSLLayer::~OpenSLLayer()
{
    stopStream();
}

//#define RECORD_AUDIO_TODISK
#ifdef RECORD_AUDIO_TODISK
#include <fstream>
std::ofstream opensl_outfile;
std::ofstream opensl_infile;
#endif

void
OpenSLLayer::startStream()
{
    if (isStarted_)
        return;

    DEBUG("Start OpenSL audio layer");

    if (audioThread_ == NULL) {
#ifdef RECORD_AUDIO_TODISK
        opensl_outfile.open("/data/data/com.savoirfairelinux.sflphone/opensl_playback.raw", std::ofstream::out | std::ofstream::binary);
        opensl_infile.open("/data/data/com.savoirfairelinux.sflphone/opensl_record.raw", std::ofstream::out | std::ofstream::binary);
#endif

        audioThread_ = new OpenSLThread(this);
        isStarted_ = true;
        audioThread_->start();
    }

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

    delete audioThread_;
    audioThread_ = NULL;
#ifdef RECORD_AUDIO_TODISK
    opensl_outfile.close();
    opensl_infile.close();
#endif
}

void
OpenSLLayer::initAudioEngine()
{
    SLresult result;

    DEBUG("Create Audio Engine\n");
    result = slCreateEngine(&engineObject_, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Realize Audio Engine\n");
    result = (*engineObject_)->Realize(engineObject_, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Create Audio Engine Interface\n");
    result = (*engineObject_)->GetInterface(engineObject_, SL_IID_ENGINE, &engineInterface_);
    assert(SL_RESULT_SUCCESS == result);

    DEBUG("Create Output Mixer\n");
    result = (*engineInterface_)->CreateOutputMix(engineInterface_, &outputMixer_, 0, NULL, NULL);
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

    if (playerObject_ != NULL) {
        (*playerObject_)->Destroy(playerObject_);
        playerObject_ = NULL;
        playerInterface_ = NULL;
        playbackBufferQueue_ = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    DEBUG("Shutdown audio mixer\n");

    if (outputMixer_ != NULL) {
        (*outputMixer_)->Destroy(outputMixer_);
        outputMixer_ = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    DEBUG("Shutdown audio engine\n");

    if (engineObject_ != NULL) {
        (*engineObject_)->Destroy(engineObject_);
        engineObject_ = NULL;
        engineInterface_ = NULL;
    }
}

void
OpenSLLayer::initAudioPlayback()
{
    assert(NULL != engineObject_);
    assert(NULL != engineInterface_);
    assert(NULL != outputMixer_);

    SLresult result;

    // Initialize the location of the buffer queue
    DEBUG("Create playback queue\n");
    SLDataLocator_AndroidSimpleBufferQueue bufferLocation = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                           NB_BUFFER_PLAYBACK_QUEUE
                                                            };

    // Initnialize the audio format for this queue
    DEBUG("Setting audio format\n");
    SLDataFormat_PCM audioFormat = {SL_DATAFORMAT_PCM, 1,
                                    SL_SAMPLINGRATE_8,
                                    SL_PCMSAMPLEFORMAT_FIXED_16,
                                    SL_PCMSAMPLEFORMAT_FIXED_16,
                                    SL_SPEAKER_FRONT_CENTER,
                                    SL_BYTEORDER_LITTLEENDIAN
                                   };

    // Create the audio source
    DEBUG("Set Audio Sources\n");
    SLDataSource audioSource = {&bufferLocation, &audioFormat};

    // Cofiguration fo the audio sink as an output mixer
    DEBUG("Set output mixer location\n");
    SLDataLocator_OutputMix mixerLocation = {SL_DATALOCATOR_OUTPUTMIX, outputMixer_};
    SLDataSink audioSink = {&mixerLocation, NULL};

    const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                 SL_IID_VOLUME, SL_IID_ANDROIDCONFIGURATION
                                };
    const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    const unsigned nbInterface = ARRAYSIZE(ids);

    SLAndroidConfigurationItf playerConfig;
    SLint32 streamType = SL_ANDROID_STREAM_VOICE;

    // create audio player
    DEBUG("Create audio player\n");
    result = (*engineInterface_)->CreateAudioPlayer(engineInterface_, &playerObject_, &audioSource, &audioSink, nbInterface, ids, req);
    assert(SL_RESULT_SUCCESS == result);

    result = (*playerObject_)->GetInterface(playerObject_,
                                            SL_IID_ANDROIDCONFIGURATION,
                                            &playerConfig);

    if (result == SL_RESULT_SUCCESS && playerConfig) {
        result = (*playerConfig)->SetConfiguration(
                     playerConfig, SL_ANDROID_KEY_STREAM_TYPE,
                     &streamType, sizeof(SLint32));
    }

    if (result != SL_RESULT_SUCCESS) {
        ERROR("Unable to set android player configuration");
    }

    DEBUG("Realize audio player\n");
    result = (*playerObject_)->Realize(playerObject_, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

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
                                            NULL
                                           };

    SLDataSource audioSource = {&deviceLocator,
                                NULL
                               };

    // configure audio sink
    DEBUG("Configure audio sink\n");

    SLDataLocator_AndroidSimpleBufferQueue bufferLocator = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                           NB_BUFFER_CAPTURE_QUEUE
                                                           };

    SLDataFormat_PCM audioFormat = {SL_DATAFORMAT_PCM, 1,
                                    SL_SAMPLINGRATE_8,
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
    const SLInterfaceID id[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};

    if (engineInterface_ != NULL) {
        result = (*engineInterface_)->CreateAudioRecorder(engineInterface_,
                 &recorderObject_, &audioSource, &audioSink, 1, id, req);
    }

    if (SL_RESULT_SUCCESS != result) {
        DEBUG("Error: could not create audio recorder");
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
    assert(NULL != playbackBufferQueue_);

    DEBUG("Start audio playback\n");

    SLresult result;

    for (int i = 0; i < NB_BUFFER_PLAYBACK_QUEUE; i++) {
        AudioBuffer &buffer = getNextPlaybackBuffer();
        incrementPlaybackIndex();

        buffer.reset();

        result = (*playbackBufferQueue_)->Enqueue(playbackBufferQueue_, buffer.data(), buffer.size());

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
    assert(NULL != playbackBufferQueue_);

    DEBUG("Start audio capture\n");

    SLresult result;


    // in case already recording, stop recording and clear buffer queue
    if (recorderInterface_ != NULL) {
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
    result = (*recorderBufferQueue_)->Enqueue(recorderBufferQueue_, buffer.data(), buffer.size());

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

    if (playerInterface_ != NULL) {
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

    if (recorderInterface_ != NULL) {
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

    return captureDeviceList;
}

std::vector<std::string>
OpenSLLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> playbackDeviceList;

    return playbackDeviceList;
}

void OpenSLLayer::playback(SLAndroidSimpleBufferQueueItf queue)
{
    assert(NULL != queue);

    usleep(20000);

    AudioBuffer &buffer = getNextPlaybackBuffer();

    buffer.reset();

    const bool bufferFilled = audioPlaybackFillBuffer(buffer);

    if (bufferFilled) {
#ifdef RECORD_AUDIO_TODISK
        opensl_outfile.write((char const *)(buffer.data()), buffer.size());
#endif
        SLresult result = (*queue)->Enqueue(queue, buffer.data(), buffer.size());

        if (SL_RESULT_SUCCESS != result) {
            DEBUG("Error could not enqueue buffers in playback callback\n");
        }

        incrementPlaybackIndex();
    }
}

void OpenSLLayer::audioPlaybackCallback(SLAndroidSimpleBufferQueueItf queue, void *context)
{
    assert(NULL != context);
    static_cast<OpenSLLayer*>(context)->playback(queue);
}

void OpenSLLayer::capture(SLAndroidSimpleBufferQueueItf queue)
{
    assert(NULL != queue);

    AudioBuffer &buffer = getNextRecordBuffer();
    incrementRecordIndex();

    SLresult result;

    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we enqueue at least 2 empty buffers to start things off)
    result = (*recorderBufferQueue_)->Enqueue(recorderBufferQueue_, buffer.data(), buffer.size());
    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    assert(SL_RESULT_SUCCESS == result);

    audioCaptureFillBuffer(buffer);
#ifdef RECORD_AUDIO_TODISK
    opensl_infile.write((char const *)(buffer.data()), buffer.size());
#endif
}

void OpenSLLayer::audioCaptureCallback(SLAndroidSimpleBufferQueueItf queue, void *context)
{
    assert(NULL != context);
    static_cast<OpenSLLayer*>(context)->capture(queue);
}


void OpenSLLayer::updatePreference(AudioPreference &preference, int index, PCMType type)
{
#ifdef OUTSIDE_TESTING

    switch (type) {
        case SFL_PCM_PLAYBACK:
            break;

        case AudioLayer::SFL_PCM_CAPTURE:
            break;

        case AudioLayer::SFL_PCM_RINGTONE:
            break;

        default:
            break;
    }

#endif
}
