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
#include "../audio_format.h"
#include "buf_manager.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

namespace jami {
namespace opensl {

/*
 * Sample Buffer Controls...
 */
#define RECORD_DEVICE_KICKSTART_BUF_COUNT 4
#define PLAY_KICKSTART_BUFFER_COUNT       8
#define DEVICE_SHADOW_BUFFER_QUEUE_LEN    4
#define BUF_COUNT                         16

inline SLAndroidDataFormat_PCM_EX
convertToSLSampleFormat(const jami::AudioFormat& infos)
{    
    if (infos.sampleFormat == AV_SAMPLE_FMT_S16)
        return SLAndroidDataFormat_PCM_EX {
            .formatType = SL_DATAFORMAT_PCM,
            .numChannels = infos.nb_channels <= 1 ? 1u : 2u,
            .sampleRate = infos.sample_rate * 1000,
            .bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16,
            .containerSize = SL_PCMSAMPLEFORMAT_FIXED_16,
            .channelMask = infos.nb_channels <= 1 ? SL_SPEAKER_FRONT_CENTER
                                                : SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            .endianness = SL_BYTEORDER_LITTLEENDIAN,
            .representation = SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT,
        };
    else if (infos.sampleFormat == AV_SAMPLE_FMT_FLT)
        return SLAndroidDataFormat_PCM_EX {
            .formatType = SL_ANDROID_DATAFORMAT_PCM_EX,
            .numChannels = infos.nb_channels <= 1 ? 1u : 2u,
            .sampleRate = infos.sample_rate * 1000,
            .bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_32,
            .containerSize = SL_PCMSAMPLEFORMAT_FIXED_32,
            .channelMask = infos.nb_channels <= 1 ? SL_SPEAKER_FRONT_CENTER
                                                : SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            .endianness = SL_BYTEORDER_LITTLEENDIAN,
            .representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT,
        };
    else
        throw std::runtime_error("Unsupported sample format");
}

#define SLASSERT(x) \
    { \
        if (SL_RESULT_SUCCESS != (x)) \
            throw std::runtime_error("OpenSLES error " + std::to_string(x)); \
    }

/*
 * Interface for player and recorder to communicate with engine
 */
#define ENGINE_SERVICE_MSG_KICKSTART_PLAYER   1
#define ENGINE_SERVICE_MSG_RETRIEVE_DUMP_BUFS 2

using EngineCallback = std::function<void()>;

} // namespace opensl
} // namespace jami
