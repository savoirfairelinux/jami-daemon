/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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

#include <mutex>

extern "C" {
struct AVAudioFifo;
}

namespace jami {

/**
 * Buffers extra samples. This is in case an input's frame size (number of samples in
 * a frame) and the output's frame size don't match. The queue will store the extra
 * samples until a frame can be read. Will call passed in callback once a frame is output.
 *
 * Works at frame-level instead of sample- or byte-level like FFmpeg's FIFO buffers.
 */
class AudioFrameResizer {
public:
    AudioFrameResizer(const AudioFormat& format, int frameSize, std::function<void(std::shared_ptr<AudioFrame>&&)> cb = {});
    ~AudioFrameResizer();

    /**
     * Gets the numbers of samples available for reading.
     */
    int samples() const;

    /**
     * Gets the format used by @queue_, input frames must match this format or enqueuing
     * will fail. Returned frames are in this format.
     */
    AudioFormat format() const;

    void setFormat(const AudioFormat& format, int frameSize);
    void setFrameSize(int frameSize);

    /**
     * Gets the number of samples per output frame.
     */
    int frameSize() const;

    /**
     * Write @frame's data to the queue. The internal buffer will be reallocated if
     * there's not enough space for @frame's samples.
     *
     * Returns the number of samples written, or negative on error.
     *
     * NOTE @frame's format must match @format_, or this will fail.
     */
    void enqueue(std::shared_ptr<AudioFrame>&& frame);

    /**
     * Notifies owner of a new frame.
     */
    std::shared_ptr<AudioFrame> dequeue();

private:
    NON_COPYABLE(AudioFrameResizer);

    /**
     * Format used for input and output audio frames.
     */
    AudioFormat format_;

    /**
     * Number of samples in each output frame.
     */
    int frameSize_;

    /**
     * Function to call once @queue_ contains enough samples to produce a frame.
     */
    std::function<void(std::shared_ptr<AudioFrame>&&)> cb_;

    /**
     * Audio queue operating on the sample level instead of byte level.
     */
    AVAudioFifo* queue_;
    int64_t nextOutputPts_ {0};
};

} // namespace jami
