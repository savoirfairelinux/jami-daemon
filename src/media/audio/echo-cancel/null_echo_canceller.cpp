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

#include "null_echo_canceller.h"

#include <cassert>

namespace jami {

NullEchoCanceller::NullEchoCanceller(AudioFormat format, unsigned frameSize)
    : EchoCanceller(format, frameSize)
{}

void
NullEchoCanceller::putRecorded(std::shared_ptr<AudioFrame>&& buf)
{
    EchoCanceller::putRecorded(std::move(buf));
};

void
NullEchoCanceller::putPlayback(const std::shared_ptr<AudioFrame>& buf)
{
    EchoCanceller::putPlayback(buf);
};

std::shared_ptr<AudioFrame>
NullEchoCanceller::getProcessed()
{
    while (recordQueue_.samples() > recordQueue_.frameSize() * 10) {
        JAMI_DBG("record overflow %d / %d", recordQueue_.samples(), frameSize_);
        recordQueue_.dequeue();
    }
    while (playbackQueue_.samples() > playbackQueue_.frameSize() * 10) {
        JAMI_DBG("playback overflow %d / %d", playbackQueue_.samples(), frameSize_);
        playbackQueue_.dequeue();
    }
    if (recordQueue_.samples() < recordQueue_.frameSize()
        || playbackQueue_.samples() < playbackQueue_.frameSize()) {
        JAMI_DBG("underflow rec: %d, play: %d fs: %d",
                 recordQueue_.samples(),
                 playbackQueue_.samples(),
                 frameSize_);
        return {};
    }

    JAMI_WARN("Processing %d samples, rec: %d, play: %d ",
              frameSize_,
              recordQueue_.samples(),
              playbackQueue_.samples());
    playbackQueue_.dequeue();
    return recordQueue_.dequeue();
};

void NullEchoCanceller::done() {};

} // namespace jami
