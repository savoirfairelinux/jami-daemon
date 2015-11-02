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

#include "audio_common.h"


SLAndroidDataFormat_PCM_EX ConvertToSLSampleFormat(const ring::AudioFormat& infos) {
    return SLAndroidDataFormat_PCM_EX {
        .formatType     = SL_DATAFORMAT_PCM,
        .numChannels    = infos.nb_channels <= 1 ? 1 : 2,
        .sampleRate     = infos.sample_rate * 1000,
        .bitsPerSample  = SL_PCMSAMPLEFORMAT_FIXED_16,
        .containerSize  = SL_PCMSAMPLEFORMAT_FIXED_16,
        .channelMask    = infos.nb_channels <= 1 ? SL_SPEAKER_FRONT_CENTER : SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
        .endianness     = SL_BYTEORDER_LITTLEENDIAN,
        .representation = 0
    };
}
