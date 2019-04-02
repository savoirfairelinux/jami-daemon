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

#include <sys/types.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "audio_common.h"
#include "buf_manager.h"
#include "noncopyable.h"

namespace jami {
namespace opensl {

class AudioRecorder {
    SLObjectItf recObjectItf_;
    SLRecordItf recItf_;
    SLAndroidSimpleBufferQueueItf recBufQueueItf_;

    jami::AudioFormat  sampleInfo_;
    AudioQueue *freeQueue_ {nullptr};         // user
    AudioQueue *recQueue_ {nullptr};          // user
    AudioQueue  devShadowQueue_ {DEVICE_SHADOW_BUFFER_QUEUE_LEN};    // owner
    uint32_t    audioBufCount;

    EngineCallback callback_ {};

public:
    explicit AudioRecorder(jami::AudioFormat, SLEngineItf engineEngine);
    ~AudioRecorder();
    NON_COPYABLE(AudioRecorder);

    bool start();
    bool stop();
    void setBufQueues(AudioQueue *freeQ, AudioQueue *recQ);
    void processSLCallback(SLAndroidSimpleBufferQueueItf bq);
    void registerCallback(EngineCallback cb) {callback_ = cb;}
    size_t dbgGetDevBufCount();
};

}
}
