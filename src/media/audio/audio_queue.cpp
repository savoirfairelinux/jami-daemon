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

#include "audio_queue.h"
#include "libav_deps.h"
#include "logger.h"

extern "C" {
#include <libavutil/audio_fifo.h>
}

namespace ring {

AudioQueue::AudioQueue(const AudioFormat& format)
    : channels_(format.nb_channels)
    , format_(format.sampleFormat)
{
    queue_ = av_audio_fifo_alloc(format.sampleFormat, format.nb_channels, 1);
}

AudioQueue::~AudioQueue()
{
    av_audio_fifo_free(queue_);
}

int
AudioQueue::getSize() const
{
    return av_audio_fifo_size(queue_);
}

AudioFormat
AudioQueue::format() const
{
    return AudioFormat(0, channels_, format_);
}

int
AudioQueue::enqueue(AudioFrame& frame)
{
    int ret = 0;
    auto f = frame.pointer();
    // reallocate in case the queue doesn't have enough remaining space
    const int remainingSpace = av_audio_fifo_space(queue_);
    if (remainingSpace < f->nb_samples) {
        if ((ret = av_audio_fifo_realloc(queue_, getSize() + f->nb_samples)) < 0) {
            RING_ERR() << "Failed to reallocate audio queue: " << libav_utils::getError(ret);
            return -1;
        }
    }

    if ((ret = av_audio_fifo_write(queue_, reinterpret_cast<void**>(f->data), f->nb_samples)) < 0) {
        RING_ERR() << "Could not write samples to audio queue: " << libav_utils::getError(ret);
        return -1;
    }

    return ret; // number of samples written
}

int
AudioQueue::dequeue(AudioFrame& frame)
{
    int ret = 0;
    auto f = frame.pointer();
    const int size = getSize();
    if (size < f->nb_samples) {
        RING_ERR() << "Not enough samples in queue to fill frame. Requested "
            << f->nb_samples << " samples, but queue only has " << size;
        return -1;
    }
    if ((ret = av_audio_fifo_read(queue_, reinterpret_cast<void**>(f->data), size)) < size) {
        RING_ERR() << "Could not read samples from queue: " << libav_utils::getError(ret);
        return -1;
    }

    return ret; // number of samples read
}

} // namespace ring
