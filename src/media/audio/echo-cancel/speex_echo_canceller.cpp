/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

#include "speex_echo_canceller.h"

#include "audio/audiolayer.h"

extern "C" {
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
}

namespace jami {

struct SpeexEchoCanceller::SpeexEchoStateImpl
{
    using SpeexEchoStatePtr = std::unique_ptr<SpeexEchoState, void (*)(SpeexEchoState*)>;
    SpeexEchoStatePtr state;

    SpeexEchoStateImpl(AudioFormat format, unsigned frameSize)
        : state(speex_echo_state_init_mc(frameSize,
                                         frameSize * 16,
                                         format.nb_channels,
                                         format.nb_channels),
                &speex_echo_state_destroy)
    {
        int sr = format.sample_rate;
        speex_echo_ctl(state.get(), SPEEX_ECHO_SET_SAMPLING_RATE, &sr);
    }
};

SpeexEchoCanceller::SpeexEchoCanceller(AudioFormat format, unsigned frameSize)
    : EchoCanceller(format, frameSize)
    , pimpl_(std::make_unique<SpeexEchoStateImpl>(format, frameSize))
{
    speex_echo_ctl(pimpl_->state.get(), SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate_);
}

void
SpeexEchoCanceller::putRecorded(std::shared_ptr<AudioFrame>&& buf)
{
    playbackQueue_.enqueue(std::move(buf));
}

void
SpeexEchoCanceller::putPlayback(const std::shared_ptr<AudioFrame>& buf)
{
    auto c = buf;
    recordQueue_.enqueue(std::move(c));
}

std::shared_ptr<AudioFrame>
SpeexEchoCanceller::getProcessed()
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
        speex_echo_cancellation(pimpl_->state.get(),
                                (const int16_t*) record->pointer()->data[0],
                                (const int16_t*) playback->pointer()->data[0],
                                (int16_t*) ret->pointer()->data[0]);
        return ret;
    }
    return {};
}

void
SpeexEchoCanceller::done()
{}

} // namespace jami
