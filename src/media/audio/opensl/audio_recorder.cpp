/*
 * Copyright 2015 The Android Open Source Project
 * Copyright 2015-2019 Savoir-faire Linux Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstring>
#include <cstdlib>
#include "audio_recorder.h"

namespace jami {
namespace opensl {

/*
 * bqRecorderCallback(): called for every buffer is full;
 *                       pass directly to handler
 */
void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *rec) {
    (static_cast<AudioRecorder *>(rec))->processSLCallback(bq);
}

void AudioRecorder::processSLCallback(SLAndroidSimpleBufferQueueItf bq) {
    try {
        assert(bq == recBufQueueItf_);
        sample_buf *dataBuf {nullptr};
        devShadowQueue_.front(&dataBuf);
        devShadowQueue_.pop();
        dataBuf->size_ = dataBuf->cap_;           //device only calls us when it is really full
        recQueue_->push(dataBuf);

        sample_buf* freeBuf;
        while (freeQueue_->front(&freeBuf) && devShadowQueue_.push(freeBuf)) {
            freeQueue_->pop();
            SLresult result = (*bq)->Enqueue(bq, freeBuf->buf_, freeBuf->cap_);
            SLASSERT(result);
        }

        // should leave the device to sleep to save power if no buffers
        if (devShadowQueue_.size() == 0) {
            (*recItf_)->SetRecordState(recItf_, SL_RECORDSTATE_STOPPED);
        }
        callback_(false);
    } catch (const std::exception& e) {
        JAMI_ERR("processSLCallback exception: %s", e.what());
    }
}

AudioRecorder::AudioRecorder(jami::AudioFormat sampleFormat, SLEngineItf slEngine) :
        sampleInfo_(sampleFormat)
{
    JAMI_DBG("Creating OpenSL record stream");

    // configure audio source/
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,
                                      SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT,
                                      nullptr };
    SLDataSource audioSrc = {&loc_dev, nullptr };

    // configure audio sink
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            DEVICE_SHADOW_BUFFER_QUEUE_LEN };

    auto format_pcm = convertToSLSampleFormat(sampleInfo_);
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID ids[] = {
        SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
        SL_IID_ANDROIDCONFIGURATION,
        SL_IID_ANDROIDACOUSTICECHOCANCELLATION,
        SL_IID_ANDROIDAUTOMATICGAINCONTROL,
        SL_IID_ANDROIDNOISESUPPRESSION
    };
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    SLresult result;
    result = (*slEngine)->CreateAudioRecorder(slEngine,
                                              &recObjectItf_,
                                              &audioSrc,
                                              &audioSnk,
                                              sizeof(ids)/sizeof(ids[0]), ids, req);
    SLASSERT(result);

    SLAndroidConfigurationItf recordConfig;
    SLint32 streamType = SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;
    result = (*recObjectItf_)->GetInterface(recObjectItf_, SL_IID_ANDROIDCONFIGURATION, &recordConfig);
    result = (*recordConfig)->SetConfiguration(recordConfig, SL_ANDROID_KEY_RECORDING_PRESET, &streamType, sizeof(SLint32));

    bool aec{true}, agc(true), ns(true);


    result = (*recObjectItf_)->Realize(recObjectItf_, SL_BOOLEAN_FALSE);
    SLASSERT(result);
    result = (*recObjectItf_)->GetInterface(recObjectItf_, SL_IID_RECORD, &recItf_);
    SLASSERT(result);


    /* Check actual performance mode granted*/
    SLuint32 modeRetrieved = SL_ANDROID_PERFORMANCE_NONE;
    SLuint32 modeSize = sizeof(SLuint32);
    result = (*recordConfig)->GetConfiguration(recordConfig, SL_ANDROID_KEY_PERFORMANCE_MODE,
            &modeSize, (void*)&modeRetrieved);
    SLASSERT(result);
    JAMI_WARN("Actual performance mode is %u\n", modeRetrieved);

    /* Enable AEC if requested */
    if (aec) {
        SLAndroidAcousticEchoCancellationItf aecItf;
        result = (*recObjectItf_)->GetInterface(recObjectItf_, SL_IID_ANDROIDACOUSTICECHOCANCELLATION, (void*)&aecItf);
        JAMI_WARN("AEC is %savailable\n", SL_RESULT_SUCCESS == result ? "" : "not ");
        if (SL_RESULT_SUCCESS == result) {
            SLboolean enabled;
            result = (*aecItf)->IsEnabled(aecItf, &enabled);
            SLASSERT(result);
            JAMI_WARN("AEC was %s\n", enabled ? "enabled" : "not enabled");

            result = (*aecItf)->SetEnabled(aecItf, true);
            SLASSERT(result);

            result = (*aecItf)->IsEnabled(aecItf, &enabled);
            SLASSERT(result);
            JAMI_WARN("AEC is now %s\n", enabled ? "enabled" : "not enabled");

            hasNativeAEC_ = enabled;
        }
    }
    /* Enable AGC if requested */
    if (agc) {
        SLAndroidAutomaticGainControlItf agcItf;
        result = (*recObjectItf_)->GetInterface(
                recObjectItf_, SL_IID_ANDROIDAUTOMATICGAINCONTROL, (void*)&agcItf);
        JAMI_WARN("AGC is %savailable\n", SL_RESULT_SUCCESS == result ? "" : "not ");
        if (SL_RESULT_SUCCESS == result) {
            SLboolean enabled;
            result = (*agcItf)->IsEnabled(agcItf, &enabled);
            SLASSERT(result);
            JAMI_WARN("AGC was %s\n", enabled ? "enabled" : "not enabled");

            result = (*agcItf)->SetEnabled(agcItf, true);
            SLASSERT(result);

            result = (*agcItf)->IsEnabled(agcItf, &enabled);
            SLASSERT(result);
            JAMI_WARN("AGC is now %s\n", enabled ? "enabled" : "not enabled");
        }
    }
    /* Enable NS if requested */
    if (ns) {
        SLAndroidNoiseSuppressionItf nsItf;
        result = (*recObjectItf_)->GetInterface(
                recObjectItf_, SL_IID_ANDROIDNOISESUPPRESSION, (void*)&nsItf);
        JAMI_WARN("NS is %savailable\n", SL_RESULT_SUCCESS == result ? "" : "not ");
        if (SL_RESULT_SUCCESS == result) {
            SLboolean enabled;
            result = (*nsItf)->IsEnabled(nsItf, &enabled);
            SLASSERT(result);
            JAMI_WARN("NS was %s\n", enabled ? "enabled" : "not enabled");

            result = (*nsItf)->SetEnabled(nsItf, true);
            SLASSERT(result);

            result = (*nsItf)->IsEnabled(nsItf, &enabled);
            SLASSERT(result);
            JAMI_WARN("NS is now %s\n", enabled ? "enabled" : "not enabled");
        }
    }

    result = (*recObjectItf_)->GetInterface(recObjectItf_, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &recBufQueueItf_);
    SLASSERT(result);

    result = (*recBufQueueItf_)->RegisterCallback(recBufQueueItf_, bqRecorderCallback, this);
    SLASSERT(result);
}

bool
AudioRecorder::start()
{
    if(!freeQueue_ || !recQueue_) {
        JAMI_ERR("====NULL pointer to Start(%p, %p)", freeQueue_, recQueue_);
        return false;
    }
    audioBufCount = 0;

    SLresult result;
    // in case already recording, stop recording and clear buffer queue
    result = (*recItf_)->SetRecordState(recItf_, SL_RECORDSTATE_STOPPED);
    SLASSERT(result);
    result = (*recBufQueueItf_)->Clear(recBufQueueItf_);
    SLASSERT(result);

    for(int i =0; i < RECORD_DEVICE_KICKSTART_BUF_COUNT; i++ ) {
        sample_buf *buf = NULL;
        if(!freeQueue_->front(&buf)) {
            JAMI_ERR("=====OutOfFreeBuffers @ startingRecording @ (%d)", i);
            break;
        }
        freeQueue_->pop();
        assert(buf->buf_ && buf->cap_ && !buf->size_);

        result = (*recBufQueueItf_)->Enqueue(recBufQueueItf_, buf->buf_,
                                                 buf->cap_);
        SLASSERT(result);
        devShadowQueue_.push(buf);
    }

    result = (*recItf_)->SetRecordState(recItf_, SL_RECORDSTATE_RECORDING);
    SLASSERT(result);

    return result == SL_RESULT_SUCCESS;
}

bool
AudioRecorder::stop()
{
    // in case already recording, stop recording and clear buffer queue
    SLuint32 curState;
    SLresult result = (*recItf_)->GetRecordState(recItf_, &curState);
    SLASSERT(result);
    if (curState == SL_RECORDSTATE_STOPPED)
        return true;

    result = (*recItf_)->SetRecordState(recItf_, SL_RECORDSTATE_STOPPED);
    SLASSERT(result);
    result = (*recBufQueueItf_)->Clear(recBufQueueItf_);
    SLASSERT(result);

    sample_buf *buf {nullptr};
    while(devShadowQueue_.front(&buf)) {
        devShadowQueue_.pop();
        freeQueue_->push(buf);
    }

    return true;
}

AudioRecorder::~AudioRecorder() {
    JAMI_DBG("Destroying OpenSL record stream");

    // destroy audio recorder object, and invalidate all associated interfaces
    if (recObjectItf_) {
        (*recObjectItf_)->Destroy(recObjectItf_);
    }
}

void AudioRecorder::setBufQueues(AudioQueue *freeQ, AudioQueue *recQ) {
    assert(freeQ && recQ);
    freeQueue_ = freeQ;
    recQueue_ = recQ;
}

size_t AudioRecorder::dbgGetDevBufCount() {
     return devShadowQueue_.size();
}

}}
