/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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
    if (synchronizeBuffers()) {
        return {};
    }

    // Compensate for persistent drift
    compensatePersistentDrift();

    playbackQueue_.dequeue();
    return recordQueue_.dequeue();
};

} // namespace jami
