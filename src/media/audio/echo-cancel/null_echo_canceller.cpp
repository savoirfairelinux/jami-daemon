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
    if (recordQueue_.samples() < recordQueue_.frameSize()
        || playbackQueue_.samples() < playbackQueue_.frameSize()) {
        JAMI_DBG("recorded underflow %d / %d", recordQueue_.samples(), frameSize_);
        return {};
    }
    while (recordQueue_.samples() > recordQueue_.frameSize()) {
        JAMI_DBG("recorded overflow %d / %d", recordQueue_.samples(), frameSize_);
        recordQueue_.dequeue();
    }
    while (playbackQueue_.samples() > 0) {
        JAMI_DBG("playback overflow %d", playbackQueue_.samples());
        playbackQueue_.dequeue();
    }
    auto recorded = recordQueue_.dequeue();
    assert(recorded->getFrameSize() == frameSize_);
    auto ret = std::make_shared<AudioFrame>(recorded->getFormat(), frameSize_);
    std::copy_n(recorded->pointer()->data[0], frameSize_, ret->pointer()->data[0]);
    return ret;
};

void NullEchoCanceller::done() {};

} // namespace jami
