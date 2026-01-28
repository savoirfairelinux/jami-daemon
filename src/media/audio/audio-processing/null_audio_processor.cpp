/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "null_audio_processor.h"

#include <cassert>

namespace jami {

NullAudioProcessor::NullAudioProcessor(AudioFormat format, unsigned frameSize)
    : AudioProcessor(format, frameSize)
{
    JAMI_DBG("[null_audio] NullAudioProcessor, frame size = %d (=%d ms), channels = %d",
             frameSize,
             frameDurationMs_,
             format.nb_channels);
}

std::shared_ptr<AudioFrame>
NullAudioProcessor::getProcessed()
{
    if (tidyQueues()) {
        return {};
    }

    playbackQueue_.dequeue();
    auto ret = recordQueue_.dequeue();

    AVFrame* frame = ret->pointer();
    JAMI_WARNING("@null frame:{} channel_layout:{} channels:{} order:{} nb_channels:{}",
                 fmt::ptr(frame),
                 frame->channel_layout,
                 frame->channels,
                 static_cast<int>(frame->ch_layout.order),
                 frame->ch_layout.nb_channels);

    return ret;
};

} // namespace jami
