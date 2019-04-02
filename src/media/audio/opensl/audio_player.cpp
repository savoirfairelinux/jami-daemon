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

#include "audio_player.h"
#include "logger.h"

#include <SLES/OpenSLES_AndroidConfiguration.h>

#include <cstdlib>

namespace jami {
namespace opensl {

/*
 * Called by OpenSL SimpleBufferQueue for every audio buffer played
 * directly pass through to our handler.
 * The regularity of this callback from openSL/Android System affects
 * playback continuity. If it does not callback in the regular time
 * slot, you are under big pressure for audio processing[here we do
 * not do any filtering/mixing]. Callback from fast audio path are
 * much more regular than other audio paths by my observation. If it
 * very regular, you could buffer much less audio samples between
 * recorder and player, hence lower latency.
 */
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *ctx) {
    (static_cast<AudioPlayer *>(ctx))->processSLCallback(bq);
}
void AudioPlayer::processSLCallback(SLAndroidSimpleBufferQueueItf bq) {
    std::lock_guard<std::mutex> lk(m_);

    // retrieve the finished device buf and put onto the free queue
    // so recorder could re-use it
    sample_buf *buf;
    if(!devShadowQueue_.front(&buf)) {
        JAMI_ERR("AudioPlayer buffer lost");
        /*
         * This should not happen: we got a callback,
         * but we have no buffer in deviceShadowedQueue
         * we lost buffers this way...(ERROR)
         */
        callback_(true);
        return;
    }
    devShadowQueue_.pop();
    buf->size_ = 0;
    if (!freeQueue_->push(buf)) {
        JAMI_ERR("buffer lost");
    }

    callback_(false);

    while(playQueue_->front(&buf) && devShadowQueue_.push(buf)) {
        if ((*bq)->Enqueue(bq, buf->buf_, buf->size_) != SL_RESULT_SUCCESS) {
            devShadowQueue_.pop();
            JAMI_ERR("enqueue failed %zu %zu %zu %zu", buf->size_, freeQueue_->size(), playQueue_->size(), devShadowQueue_.size());
            break;
        } else
            playQueue_->pop();
    }
    if (devShadowQueue_.size() == 0) {
        JAMI_ERR("AudioPlayer: nothing to play %zu %zu %zu", freeQueue_->size(), playQueue_->size(), devShadowQueue_.size());
        waiting_ = true;
        callback_(true);
    }
}

AudioPlayer::AudioPlayer(jami::AudioFormat sampleFormat, SLEngineItf slEngine, SLint32 streamType) :
    sampleInfo_(sampleFormat)
{
    SLresult result;
    result = (*slEngine)->CreateOutputMix(slEngine, &outputMixObjectItf_, 0, nullptr, nullptr);
    SLASSERT(result);

    // realize the output mix
    result = (*outputMixObjectItf_)->Realize(outputMixObjectItf_, SL_BOOLEAN_FALSE);
    SLASSERT(result);

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            DEVICE_SHADOW_BUFFER_QUEUE_LEN };

    auto format_pcm = convertToSLSampleFormat(sampleInfo_);
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObjectItf_};
    SLDataSink audioSnk = {&loc_outmix, nullptr};
    /*
     * create fast path audio player: SL_IID_BUFFERQUEUE and SL_IID_VOLUME interfaces ok,
     * NO others!
     */
    SLInterfaceID  ids[3] = { SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_ANDROIDCONFIGURATION};
    SLboolean      req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*slEngine)->CreateAudioPlayer(slEngine, &playerObjectItf_, &audioSrc, &audioSnk,
                                            sizeof(ids)/sizeof(ids[0]), ids, req);
    SLASSERT(result);

    SLAndroidConfigurationItf playerConfig;
    result = (*playerObjectItf_)->GetInterface(playerObjectItf_, SL_IID_ANDROIDCONFIGURATION, &playerConfig);
    result = (*playerConfig)->SetConfiguration(playerConfig, SL_ANDROID_KEY_STREAM_TYPE, &streamType, sizeof(SLint32));

    // realize the player
    result = (*playerObjectItf_)->Realize(playerObjectItf_, SL_BOOLEAN_FALSE);
    SLASSERT(result);

    // get the play interface
    result = (*playerObjectItf_)->GetInterface(playerObjectItf_, SL_IID_PLAY, &playItf_);
    SLASSERT(result);

    // get the buffer queue interface
    result = (*playerObjectItf_)->GetInterface(playerObjectItf_, SL_IID_BUFFERQUEUE, &playBufferQueueItf_);
    SLASSERT(result);

    // register callback on the buffer queue
    result = (*playBufferQueueItf_)->RegisterCallback(playBufferQueueItf_, bqPlayerCallback, this);
    SLASSERT(result);

    result = (*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_STOPPED);
    SLASSERT(result);
}

AudioPlayer::~AudioPlayer() {

    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (playerObjectItf_) {
        (*playerObjectItf_)->Destroy(playerObjectItf_);
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (outputMixObjectItf_) {
        (*outputMixObjectItf_)->Destroy(outputMixObjectItf_);
    }
}

void AudioPlayer::setBufQueue(AudioQueue *playQ, AudioQueue *freeQ) {
    playQueue_ = playQ;
    freeQueue_ = freeQ;
}

bool AudioPlayer::start() {
    SLuint32   state;
    SLresult  result = (*playItf_)->GetPlayState(playItf_, &state);
    if (result != SL_RESULT_SUCCESS)
        return false;
    if(state == SL_PLAYSTATE_PLAYING)
        return true;

    result = (*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_STOPPED);
    SLASSERT(result);

    result = (*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_PLAYING);
    SLASSERT(result);

    // send pre-defined audio buffers to device
    int i = PLAY_KICKSTART_BUFFER_COUNT;
    while(i--) {
        sample_buf *buf;
        if(!playQueue_->front(&buf))    //we have buffers for sure
            break;
        if(SL_RESULT_SUCCESS !=
           (*playBufferQueueItf_)->Enqueue(playBufferQueueItf_, buf, buf->size_))
        {
            JAMI_ERR("====failed to enqueue (%d) in %s", i, __FUNCTION__);
            return false;
        } else {
            playQueue_->pop();
            devShadowQueue_.push(buf);
        }
    }
    waiting_ = devShadowQueue_.size() == 0;
    return true;
}

bool AudioPlayer::started() const
{
    if (!playItf_)
        return false;
    SLuint32 state;
    SLresult result = (*playItf_)->GetPlayState(playItf_, &state);
    return result == SL_RESULT_SUCCESS && state == SL_PLAYSTATE_PLAYING;
}

void AudioPlayer::stop() {
    SLuint32   state;

    SLresult   result = (*playItf_)->GetPlayState(playItf_, &state);
    SLASSERT(result);

    if(state == SL_PLAYSTATE_STOPPED)
        return;

    result = (*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_STOPPED);
    SLASSERT(result);

    waiting_ = false;

    // Consume all non-completed audio buffers
    sample_buf *buf = nullptr;
    while(devShadowQueue_.front(&buf)) {
        buf->size_ = 0;
        devShadowQueue_.pop();
        freeQueue_->push(buf);
    }
    while(playQueue_->front(&buf)) {
        buf->size_ = 0;
        playQueue_->pop();
        freeQueue_->push(buf);
    }
}

void AudioPlayer::playAudioBuffers(unsigned count) {
    while (count--) {
        sample_buf *buf = nullptr;
        if(!playQueue_->front(&buf)) {
            JAMI_ERR("====Run out of buffers in %s @(count = %d)", __FUNCTION__, count);
            break;
        }
        if(!devShadowQueue_.push(buf)) {
            break;  // PlayerBufferQueue is full!!!
        }

        SLresult result = (*playBufferQueueItf_)->Enqueue(playBufferQueueItf_,
                                                  buf->buf_, buf->size_);
        if(result != SL_RESULT_SUCCESS) {
            JAMI_ERR("%s Error @( %p, %d ), result = %d", __FUNCTION__, (void*)buf->buf_, buf->size_, result);
            /*
             * when this happens, a buffer is lost. Need to remove the buffer
             * from top of the devShadowQueue. Since I do not have it now,
             * just pop out the one that is being played right now. Afer a
             * cycle it will be normal.
             */
            devShadowQueue_.front(&buf), devShadowQueue_.pop();
            freeQueue_->push(buf);
            break;
        }
        playQueue_->pop();   // really pop out the buffer
    }
    waiting_ = devShadowQueue_.size() == 0;
}

size_t AudioPlayer::dbgGetDevBufCount(void) {
    return devShadowQueue_.size();
}

}}
