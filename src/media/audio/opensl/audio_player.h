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

#pragma once

#include "audio_common.h"
#include "buf_manager.h"
#include "noncopyable.h"

#include <sys/types.h>
#include <SLES/OpenSLES_Android.h>

#include <mutex>

namespace jami {
namespace opensl {

class AudioPlayer {
    // buffer queue player interfaces
    SLObjectItf outputMixObjectItf_;
    SLObjectItf playerObjectItf_;
    SLPlayItf   playItf_;
    SLAndroidSimpleBufferQueueItf playBufferQueueItf_;

    jami::AudioFormat sampleInfo_;
    AudioQueue *freeQueue_ {nullptr};       // user
    AudioQueue *playQueue_ {nullptr};       // user
    AudioQueue devShadowQueue_ {DEVICE_SHADOW_BUFFER_QUEUE_LEN};  // owner

    EngineCallback callback_ {};

public:
    explicit AudioPlayer(jami::AudioFormat sampleFormat, SLEngineItf engine, SLint32 streamType);
    ~AudioPlayer();
    NON_COPYABLE(AudioPlayer);

    bool start();
    void stop();
    bool started() const;

    void setBufQueue(AudioQueue *playQ, AudioQueue *freeQ);
    void processSLCallback(SLAndroidSimpleBufferQueueItf bq);
    void playAudioBuffers(unsigned count);
    void registerCallback(EngineCallback cb) {callback_ = cb;}
    size_t dbgGetDevBufCount();

    std::mutex m_;
    std::atomic_bool waiting_ {false};
};

}}
