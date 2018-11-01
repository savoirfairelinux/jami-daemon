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

#include "audio_frame_resizer.h"
#include "libav_deps.h"
#include "logger.h"

extern "C" {
#include <libavutil/audio_fifo.h>
}

#include <stdexcept>

namespace ring {

AudioFrameResizer::AudioFrameResizer(const AudioFormat& format, int frameSize, std::function<void(std::unique_ptr<AudioFrame>&&)> cb)
    : format_(format)
    , frameSize_(frameSize)
    , cb_(cb)
{
    // NOTE 160 samples should the minimum that will be provided (20 ms @ 8kHz),
    // barring files that for some obscure reason have smaller packets
    queue_ = av_audio_fifo_alloc(format.sampleFormat, format.nb_channels, 160);
}

AudioFrameResizer::~AudioFrameResizer()
{
    av_audio_fifo_free(queue_);
}

int
AudioFrameResizer::samples() const
{
    return av_audio_fifo_size(queue_);
}

int
AudioFrameResizer::frameSize() const
{
    return frameSize_;
}

AudioFormat
AudioFrameResizer::format() const
{
    return format_;
}

void
AudioFrameResizer::enqueue(std::unique_ptr<AudioFrame>&& frame)
{
    int ret = 0;
    auto f = frame->pointer();
    if (f->format != (int)format_.sampleFormat || f->channels != (int)format_.nb_channels || f->sample_rate != (int)format_.sample_rate) {
        RING_ERR() << "Expected " << format_ << ", but got " << AudioFormat(f->sample_rate, f->channels, (AVSampleFormat)f->format);
        throw std::runtime_error("Could not write samples to audio queue: input frame is not the right format");
    }

    if (samples() == 0 && f->nb_samples == frameSize_) {
        cb_(std::move(frame));
        return; // return if frame was just passed through
    }

    // queue reallocates itself if need be
    if ((ret = av_audio_fifo_write(queue_, reinterpret_cast<void**>(f->data), f->nb_samples)) < 0) {
        RING_ERR() << "Audio resizer error: " << libav_utils::getError(ret);
        throw std::runtime_error("Failed to add audio to frame resizer");
    }

    while (auto frame = dequeue())
        cb_(std::move(frame));
}

std::unique_ptr<AudioFrame>
AudioFrameResizer::dequeue()
{
    if (samples() < frameSize_)
        return {};

    int ret;
    auto frame = std::make_unique<AudioFrame>();
    auto f = frame->pointer();
    f->format = (int)format_.sampleFormat;
    f->channels = format_.nb_channels;
    f->channel_layout = av_get_default_channel_layout(format_.nb_channels);
    f->sample_rate = format_.sample_rate;
    f->nb_samples = frameSize_;
    if ((ret = av_frame_get_buffer(f, 0)) < 0) {
        RING_ERR() << "Failed to allocate audio buffers: " << libav_utils::getError(ret);
        return {};
    }

    if ((ret = av_audio_fifo_read(queue_, reinterpret_cast<void**>(f->data), frameSize_)) < 0) {
        RING_ERR() << "Could not read samples from queue: " << libav_utils::getError(ret);
        return {};
    }

    return frame;
}

} // namespace ring
