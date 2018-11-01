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
    : format_(format)
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
    return format_;
}

int
AudioQueue::enqueue(AudioFrame& frame)
{
    int ret = 0;
    auto f = frame.pointer();
    if (f->format != (int)format_.sampleFormat || f->channels != (int)format_.nb_channels || f->sample_rate != (int)format_.sample_rate) {
        RING_ERR() << "Could not write samples to audio queue: input frame is not the right format";
        return -1;
    }

    // queue reallocates itself if need be
    if ((ret = av_audio_fifo_write(queue_, reinterpret_cast<void**>(f->data), f->nb_samples)) < 0) {
        RING_ERR() << "Could not write samples to audio queue: " << libav_utils::getError(ret);
        return -1;
    }

    return ret; // number of samples written
}

std::unique_ptr<AudioFrame>
AudioQueue::dequeue(int nbSamples)
{
    // normal case, don't print anything
    if (getSize() < nbSamples)
        return {};

    int ret;
    auto frame = std::make_unique<AudioFrame>();
    auto f = frame->pointer();
    f->format = (int)format_.sampleFormat;
    f->channels = format_.nb_channels;
    f->channel_layout = av_get_default_channel_layout(format_.nb_channels);
    f->sample_rate = format_.sample_rate;
    f->nb_samples = nbSamples;
    if ((ret = av_frame_get_buffer(f, 0)) < 0) {
        RING_ERR() << "Failed to allocate audio buffers: " << libav_utils::getError(ret);
        return {};
    }

    if ((ret = av_audio_fifo_read(queue_, reinterpret_cast<void**>(f->data), nbSamples)) < 0) {
        RING_ERR() << "Could not read samples from queue: " << libav_utils::getError(ret);
        return {};
    }

    return frame;
}

} // namespace ring
