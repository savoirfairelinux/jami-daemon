/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#pragma once

#include "audiobuffer.h"
#include "media_buffer.h"
#include "noncopyable.h"

struct AVAudioFifo;

namespace ring {

/**
 * Buffers extra samples. This is in case an input's frame size (number of samples in
 * a frame) and the output's frame size don't match. The queue will store the extra
 * samples until a frame can be read.
 *
 * Works at frame-level instead of sample- or byte-level like FFmpeg's FIFO buffers.
 */
class AudioQueue {
public:
    AudioQueue(const AudioFormat& format);
    ~AudioQueue();

    /**
     * Gets the numbers of samples available for reading.
     */
    int getSize() const;

    /**
     * Gets the internal format used in @queue_, which is also the format of the frames
     * returned by dequeue.
     */
    AudioFormat format() const;

    /**
     * Write @frame's data to the queue. The internal buffer will be reallocated if
     * there's not enough space for @frame's samples.
     *
     * Returns the number of samples written, or negative on error.
     *
     * NOTE @frame's format must match @format_
     */
    int enqueue(AudioFrame& frame);

    std::unique_ptr<AudioFrame> dequeue(int nbSamples);

private:
    NON_COPYABLE(AudioQueue);

    /**
     * @queue_ internal format.
     */
    AudioFormat format_;

    /**
     * Audio queue operating on the sample level instead of byte level.
     */
    AVAudioFifo* queue_;
};

} // namespace ring
