/*
 * Copyright 2015 The Android Open Source Project
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

namespace ring {
namespace opensl {

/*
 * bqRecorderCallback(): called for every buffer is full;
 *                       pass directly to handler
 */
void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *rec) {
    (static_cast<AudioRecorder *>(rec))->processSLCallback(bq);
}

void AudioRecorder::processSLCallback(SLAndroidSimpleBufferQueueItf bq) {
#ifdef ENABLE_LOG
    recLog_->logTime();
#endif
    assert(bq == recBufQueueItf_);
    sample_buf *dataBuf = NULL;
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

    /*
     * PLAY_KICKSTART_BUFFER_COUNT: # of buffers cached in the queue before
     * STARTING player. it is defined in audio_common.h. Whatever buffered
     * here is the part of the audio LATENCY! adjust to fit your bill [ until
     * it busts ]
     */
    if(++audioBufCount == PLAY_KICKSTART_BUFFER_COUNT && callback_) {
        callback_(ctx_, ENGINE_SERVICE_MSG_KICKSTART_PLAYER, NULL);
    }

    // should leave the device to sleep to save power if no buffers
    if (devShadowQueue_.size() == 0) {
        (*recItf_)->SetRecordState(recItf_, SL_RECORDSTATE_STOPPED);
    }
    callback_(ctx_, 0, nullptr);
}

AudioRecorder::AudioRecorder(ring::AudioFormat sampleFormat, SLEngineItf slEngine) :
        freeQueue_(nullptr), recQueue_(nullptr), devShadowQueue_(DEVICE_SHADOW_BUFFER_QUEUE_LEN),
        callback_(nullptr), sampleInfo_(sampleFormat)
{
    SLAndroidDataFormat_PCM_EX format_pcm = ConvertToSLSampleFormat(sampleInfo_);

    // configure audio source
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,
                                      SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT,
                                      NULL };
    SLDataSource audioSrc = {&loc_dev, NULL };

    // configure audio sink
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            DEVICE_SHADOW_BUFFER_QUEUE_LEN };

    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    SLresult result;
    result = (*slEngine)->CreateAudioRecorder(slEngine,
                                              &recObjectItf_,
                                              &audioSrc,
                                              &audioSnk,
                                              1, id, req);
    SLASSERT(result);

    result = (*recObjectItf_)->Realize(recObjectItf_, SL_BOOLEAN_FALSE);
    SLASSERT(result);
    result = (*recObjectItf_)->GetInterface(recObjectItf_,
                    SL_IID_RECORD, &recItf_);
    SLASSERT(result);

    result = (*recObjectItf_)->GetInterface(recObjectItf_,
                    SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &recBufQueueItf_);
    SLASSERT(result);

    result = (*recBufQueueItf_)->RegisterCallback(recBufQueueItf_,
                    bqRecorderCallback, this);
    SLASSERT(result);

#ifdef ENABLE_LOG
    std::string name = "rec";
    recLog_ = new AndroidLog(name);
#endif
}

SLboolean AudioRecorder::start(void) {
    if(!freeQueue_ || !recQueue_) {
        LOGE("====NULL poiter to Start(%p, %p)", freeQueue_, recQueue_);
        return SL_BOOLEAN_FALSE;
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
            LOGE("=====OutOfFreeBuffers @ startingRecording @ (%d)", i);
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

    return (result == SL_RESULT_SUCCESS? SL_BOOLEAN_TRUE:SL_BOOLEAN_FALSE);
}

SLboolean  AudioRecorder::stop(void) {
    // in case already recording, stop recording and clear buffer queue
    SLuint32 curState;

    SLresult result = (*recItf_)->GetRecordState(recItf_, &curState);
    SLASSERT(result);
    if( curState == SL_RECORDSTATE_STOPPED) {
        return SL_BOOLEAN_TRUE;
    }
    result = (*recItf_)->SetRecordState(recItf_, SL_RECORDSTATE_STOPPED);
    SLASSERT(result);
    result = (*recBufQueueItf_)->Clear(recBufQueueItf_);
    SLASSERT(result);

    sample_buf *buf = NULL;
    while(devShadowQueue_.front(&buf)) {
        devShadowQueue_.pop();
        freeQueue_->push(buf);
    }

#ifdef ENABLE_LOG
    recLog_->flush();
#endif

    return SL_BOOLEAN_TRUE;
}

AudioRecorder::~AudioRecorder() {
    // destroy audio recorder object, and invalidate all associated interfaces
    if (recObjectItf_ != NULL) {
        (*recObjectItf_)->Destroy(recObjectItf_);
    }

#ifdef  ENABLE_LOG
    if(recLog_) {
        delete recLog_;
    }
#endif
}

void AudioRecorder::setBufQueues(AudioQueue *freeQ, AudioQueue *recQ) {
    assert(freeQ && recQ);
    freeQueue_ = freeQ;
    recQueue_ = recQ;
}

void AudioRecorder::registerCallback(ENGINE_CALLBACK cb, void *ctx) {
    callback_ = cb;
    ctx_ = ctx;
}
int32_t AudioRecorder::dbgGetDevBufCount(void) {
     return devShadowQueue_.size();
}

}}
