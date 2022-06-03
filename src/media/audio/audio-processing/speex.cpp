/*
 *  Copyright (C) 2021-2022 Savoir-faire Linux Inc.
 *
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "speex.h"

#include "audio/audiolayer.h"

extern "C" {
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
}

namespace jami {

SpeexAudioProcessor::SpeexAudioProcessor(AudioFormat format, unsigned frameSize)
    : AudioProcessor(format, frameSize)
    , state(speex_echo_state_init_mc(frameSize,
                                        frameSize * 16,
                                        format.nb_channels,
                                        format.nb_channels),
            &speex_echo_state_destroy)
{
    speex_echo_ctl(state.get(), SPEEX_ECHO_SET_SAMPLING_RATE, &format_.sample_rate);
}

std::shared_ptr<AudioFrame>
SpeexAudioProcessor::getProcessed()
{
    if (playbackQueue_.samples() < playbackQueue_.frameSize()
        or recordQueue_.samples() < recordQueue_.frameSize()) {
        JAMI_DBG("getRecorded underflow %d / %d, %d / %d",
                 playbackQueue_.samples(),
                 playbackQueue_.frameSize(),
                 recordQueue_.samples(),
                 recordQueue_.frameSize());
        return {};
    }
    if (recordQueue_.samples() > 2 * recordQueue_.frameSize() && playbackQueue_.samples() == 0) {
        JAMI_DBG("getRecorded PLAYBACK underflow");
        return recordQueue_.dequeue();
    }
    while (playbackQueue_.samples() > 10 * playbackQueue_.frameSize()) {
        JAMI_DBG("getRecorded record underflow");
        playbackQueue_.dequeue();
    }
    while (recordQueue_.samples() > 4 * recordQueue_.frameSize()) {
        JAMI_DBG("getRecorded playback underflow");
        recordQueue_.dequeue();
    }
    auto playback = playbackQueue_.dequeue();
    auto record = recordQueue_.dequeue();
    if (playback and record) {
        auto ret = std::make_shared<AudioFrame>(record->getFormat(), record->getFrameSize());
        speex_echo_cancellation(state.get(),
                                (const int16_t*) record->pointer()->data[0],
                                (const int16_t*) playback->pointer()->data[0],
                                (int16_t*) ret->pointer()->data[0]);
        return ret;
    }
    return {};
}

} // namespace jami
