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

#include <stdio.h>
#include <assert.h>

#include "manager.h"
#include "mainbuffer.h"
#include "opensllayer.h"

const int OpenSLLayer::NB_BUFFER_PLAYBACK_QUEUE = ANDROID_BUFFER_QUEUE_LENGTH;
const int OpenSLLayer::NB_BUFFER_CAPTURE_QUEUE = ANDROID_BUFFER_QUEUE_LENGTH;

static long sawPtr = 0;
static void generateSawTooth(short *buffer, int length) {
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
{
    MainBuffer &buffer = Manager::instance().getMainBuffer();
}

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
    // opensl_->startAudioCapture();

    while (opensl_->isStarted_) {
        ost::Thread::sleep(20 /* ms */);
    }
}

// Constructor
OpenSLLayer::OpenSLLayer()
    : indexIn_(0)
    , indexOut_(0)
    , indexRing_(0)
    , audioThread_(NULL)
    , isStarted_(false)
    , engineObject(NULL)
    , engineInterface(NULL)
    , outputMixer(NULL)
    , playerObject(NULL)
    , recorderObject(NULL)
    , playerInterface(NULL)
    , recorderInterface(NULL)
    , playbackBufferQueue(NULL)
    , recorderBufferQueue(NULL)
    , playbackBufferIndex(0)
    , recordBufferIndex(0)
    , playbackBufferStack(ANDROID_BUFFER_QUEUE_LENGTH)
    , recordBufferStack(ANDROID_BUFFER_QUEUE_LENGTH)
{
}

// Destructor
OpenSLLayer::~OpenSLLayer()
{
    stopStream();
}

// #define RECORD_AUDIO_TODISK
#ifdef RECORD_AUDIO_TODISK
#include <fstream>
std::ofstream opensl_outfile;
#endif

void
OpenSLLayer::startStream()
{
    if(isStarted_)
        return;

    if (audioThread_ == NULL) {
#ifdef RECORD_AUDIO_TODISK
        opensl_outfile.open("/data/data/opensl_playback.raw", std::ofstream::out | std::ofstream::binary);
#endif

        audioThread_ = new OpenSLThread(this);
        isStarted_ = true;
        audioThread_->start();
    }

}

void
OpenSLLayer::stopStream()
{
    if(not isStarted_)
        return;

    isStarted_ = false;

    delete audioThread_;
    audioThread_ = NULL;
#ifdef RECORD_AUDIO_TODISK
    opensl_outfile.close();
#endif
}

void
OpenSLLayer::initAudioEngine()
{
    SLresult result;

    printf("Create Audio Engine\n");
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);

    printf("Realize Audio Engine\n");
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    printf("Create Audio Engine Interface\n");
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    assert(SL_RESULT_SUCCESS == result);

    printf("Create Output Mixer\n");
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixer, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);

    printf("Realize Output Mixer\n");
    result = (*outputMixer)->Realize(outputMixer, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    printf("Audio Engine Initialization Done\n");
}

void 
OpenSLLayer::shutdownAudioEngine()
{

    // destroy buffer queue audio player object, and invalidate all associated interfaces
    printf("Shutdown audio player\n");
    if (playerObject != NULL) {
        (*playerObject)->Destroy(playerObject);
        playerObject = NULL;
        playerInterface = NULL;
        playbackBufferQueue = NULL;
    }
        
    // destroy output mix object, and invalidate all associated interfaces
    printf("Shutdown audio mixer\n");
    if (outputMixer != NULL) {
        (*outputMixer)->Destroy(outputMixer);
        outputMixer= NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    printf("Shutdown audio engine\n");
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineInterface = NULL;
    }   
}

void
OpenSLLayer::initAudioPlayback()
{
    assert(NULL != engineObject);
    assert(NULL != engineInterface);
    assert(NULL != outputMixer);

    SLresult result;

    // Initialize the location of the buffer queue
    printf("Create playback queue\n");
    SLDataLocator_AndroidSimpleBufferQueue bufferLocation = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                             NB_BUFFER_PLAYBACK_QUEUE}; 

    // Initnialize the audio format for this queue
    printf("Setting audio format\n");
    SLDataFormat_PCM audioFormat = {SL_DATAFORMAT_PCM, 1, 
                                    SL_SAMPLINGRATE_8,
                                    SL_PCMSAMPLEFORMAT_FIXED_16, 
                                    SL_PCMSAMPLEFORMAT_FIXED_16,
                                    SL_SPEAKER_FRONT_CENTER, 
                                    SL_BYTEORDER_LITTLEENDIAN};

    // Create the audio source
    printf("Set Audio Sources\n");
    SLDataSource audioSource = {&bufferLocation, &audioFormat};

    // Cofiguration fo the audio sink as an output mixer 
    printf("Set output mixer location\n");
    SLDataLocator_OutputMix mixerLocation = {SL_DATALOCATOR_OUTPUTMIX, outputMixer};
    SLDataSink audioSink = {&mixerLocation, NULL};

    const SLInterfaceID ids[2] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, 
                                  SL_IID_VOLUME};
    const SLboolean req[2] = {SL_BOOLEAN_TRUE, 
                              SL_BOOLEAN_TRUE};

    int nbInterface = 2;

    // create audio player
    printf("Create audio player\n");
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, &playerObject, &audioSource, &audioSink, nbInterface, ids, req);
    assert(SL_RESULT_SUCCESS == result);

    printf("Realize audio player\n");
    result = (*playerObject)->Realize(playerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    // create audio interface
    printf("Create audio player interface\n");
    result = (*playerObject)->GetInterface(playerObject, SL_IID_PLAY, &playerInterface);
    assert(SL_RESULT_SUCCESS == result);

    // create the buffer queue interface
    printf("Create buffer queue interface\n");
    result = (*playerObject)->GetInterface(playerObject, SL_IID_BUFFERQUEUE, &playbackBufferQueue);
    assert(SL_RESULT_SUCCESS == result);

    // register the buffer queue on the buffer object
    printf("Register audio callback\n");
    result = (*playbackBufferQueue)->RegisterCallback(playbackBufferQueue, audioPlaybackCallback, this);
    assert(SL_RESULT_SUCCESS == result);

    printf("Audio Playback Initialization Done\n");
}

void
OpenSLLayer::initAudioCapture()
{
    SLresult result;

    // configure audio source
    printf("Configure audio source\n");
    SLDataLocator_IODevice deviceLocator = {SL_DATALOCATOR_IODEVICE, 
                                            SL_IODEVICE_AUDIOINPUT,
                                            SL_DEFAULTDEVICEID_AUDIOINPUT, 
                                            NULL};

    SLDataSource audioSource = {&deviceLocator,
                                NULL};

    // configure audio sink
    printf("Configure audio sink\n");
    SLDataLocator_AndroidSimpleBufferQueue bufferLocator = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 
                                                            NB_BUFFER_CAPTURE_QUEUE};

    SLDataFormat_PCM audioFormat = {SL_DATAFORMAT_PCM, 1, 
                                    SL_SAMPLINGRATE_8,
                                    SL_PCMSAMPLEFORMAT_FIXED_16, 
                                    16,
                                    SL_SPEAKER_FRONT_CENTER, 
                                    SL_BYTEORDER_LITTLEENDIAN};

    SLDataSink audioSink = {&bufferLocator, 
                            &audioFormat};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    printf("Create audio recorder\n");
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*engineInterface)->CreateAudioRecorder(engineInterface, &recorderObject, &audioSource,
                                                       &audioSink, 1, id, req);
    if (SL_RESULT_SUCCESS != result) {
        printf("Error: could not create audio recorder");
        return;
    }

    // realize the audio recorder
    printf("Realize the audio recorder\n");
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        printf("Error: could not realize audio recorder");
        return;
    }

    // get the record interface
    printf("Create the record interface\n");
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderInterface);
    assert(SL_RESULT_SUCCESS == result);

    // get the buffer queue interface
    printf("Create the buffer queue interface\n");
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            &recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);

    // register callback on the buffer queue
    printf("Register the audio capture callback\n");
    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, audioCaptureCallback, this);
    assert(SL_RESULT_SUCCESS == result);

    printf("Audio capture initialized\n");
}



void 
OpenSLLayer::startAudioPlayback()
{
    assert(NULL != playbackBufferQueue);

    printf("Start audio playback\n");

    SLresult result;

    for(int i = 0; i < NB_BUFFER_PLAYBACK_QUEUE; i++) {
        AudioBuffer &buffer = getNextPlaybackBuffer();
        incrementPlaybackIndex();

        // generateSawTooth(&(*buffer.begin()), buffer.size());
        memset(buffer.data(), 0, buffer.size());

        result = (*playbackBufferQueue)->Enqueue(playbackBufferQueue, buffer.data(), buffer.size());
        if (SL_RESULT_SUCCESS != result) {
            printf("Error could not enqueue initial buffers\n");
        }
    }

    result = (*playerInterface)->SetPlayState(playerInterface, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);

    printf("Audio playback started\n");
}

void
OpenSLLayer::startAudioCapture()
{
    assert(NULL != playbackBufferQueue);

    printf("Start audio capture\n");

    SLresult result;

    // in case already recording, stop recording and clear buffer queue
    result = (*recorderInterface)->SetRecordState(recorderInterface, SL_RECORDSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
    result = (*recorderBufferQueue)->Clear(recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);

    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we enqueue at least 2 empty buffers to start things off)
    AudioBuffer &buffer = getNextRecordBuffer();
    incrementRecordIndex();

    memset(buffer.data(), 0, buffer.size());

    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, buffer.data(), buffer.size());
    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    assert(SL_RESULT_SUCCESS == result);

    // start recording
    result = (*recorderInterface)->SetRecordState(recorderInterface, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);

    printf("Audio capture started\n");
}

void
OpenSLLayer::stopAudioPlayback()
{
    printf("Stop audio playback\n");

    SLresult result;
    result = (*playerInterface)->SetPlayState(playerInterface, SL_PLAYSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
}

void
OpenSLLayer::stopAudioCapture()
{
    printf("Stop audio capture\n");

    SLresult result;
    result = (*recorderInterface)->SetRecordState(recorderInterface, SL_RECORDSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
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

bool OpenSLLayer::audioCallback()
{
}

void OpenSLLayer::audioPlaybackCallback(SLAndroidSimpleBufferQueueItf queue, void *context)
{
    assert(NULL != queue);
    assert(NULL != context);

    usleep(20000);

    OpenSLLayer *opensl = (OpenSLLayer *)context;

    AudioBuffer &buffer = opensl->getNextPlaybackBuffer();

    memset(buffer.data(), 0, buffer.size());

    bool bufferFilled = opensl->audioPlaybackFillBuffer(buffer);


    if(bufferFilled) {
#ifdef RECORD_AUDIO_TODISK
        opensl_outfile.write((char const *)(buffer.data()), buffer.size());
#endif
        SLresult result = (*queue)->Enqueue(queue, buffer.data(), buffer.size());
        if (SL_RESULT_SUCCESS != result) {
            printf("Error could not enqueue buffers in playback callback\n");
        }

        opensl->incrementPlaybackIndex();
    }
}

void OpenSLLayer::audioCaptureCallback(SLAndroidSimpleBufferQueueItf queue, void *context)
{
    assert(NULL != queue);
    assert(NULL != context);

    OpenSLLayer *opensl = (OpenSLLayer *)context;

    AudioBuffer &buffer = opensl->getNextRecordBuffer();
    opensl->incrementRecordIndex();

    SLRecordItf recorderInterface = opensl->getRecorderInterface();
    SLAndroidSimpleBufferQueueItf recorderBufferQueue = opensl->getRecorderBufferQueue();

    SLresult result;

    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we enqueue at least 2 empty buffers to start things off)
    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, buffer.data(), buffer.size());
    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    assert(SL_RESULT_SUCCESS == result);

    AudioBuffer &previousbuffer = opensl->getNextRecordBuffer();

    opensl->audioCaptureFillBuffer(previousbuffer);
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
