/*
 * Copyright 2015 The Android Open Source Project
 * Copyright 2015-2023 Savoir-faire Linux Inc.
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

class AudioRecorder
{
    SLObjectItf recObjectItf_;
    SLRecordItf recItf_;
    SLAndroidSimpleBufferQueueItf recBufQueueItf_;

    jami::AudioFormat sampleInfo_;
    AudioQueue* freeQueue_ {nullptr};                            // user
    AudioQueue* recQueue_ {nullptr};                             // user
    AudioQueue devShadowQueue_ {DEVICE_SHADOW_BUFFER_QUEUE_LEN}; // owner
    uint32_t audioBufCount;
    sample_buf silentBuf_;

    EngineCallback callback_ {};
    bool hasNativeAEC_ {false};
    bool hasNativeNS_ {false};

public:
    explicit AudioRecorder(jami::AudioFormat, size_t bufSize, SLEngineItf engineEngine);
    ~AudioRecorder();
    NON_COPYABLE(AudioRecorder);

    bool start();
    bool stop();
    void setBufQueues(AudioQueue* freeQ, AudioQueue* recQ);
    void processSLCallback(SLAndroidSimpleBufferQueueItf bq);
    void registerCallback(EngineCallback cb) { callback_ = cb; }
    size_t dbgGetDevBufCount();

    bool hasNativeAEC() const { return hasNativeAEC_; }
    bool hasNativeNS() const { return hasNativeNS_; }
};

} // namespace opensl
} // namespace jami
