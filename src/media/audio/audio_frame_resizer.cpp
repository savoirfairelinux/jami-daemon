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

#include "audio_frame_resizer.h"
#include "libav_deps.h"
#include "logger.h"

extern "C" {
#include <libavutil/audio_fifo.h>
}

#include <stdexcept>

namespace ring {

AudioFrameResizer::AudioFrameResizer(const AudioFormat& format, int size, std::function<void(std::shared_ptr<AudioFrame>&&)> cb)
    : format_(format)
    , frameSize_(size)
    , cb_(cb)
    , queue_(av_audio_fifo_alloc(format.sampleFormat, format.nb_channels, frameSize_))
{}

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
AudioFrameResizer::setFormat(const AudioFormat& format, int size)
{
    if (format != format_) {
        if (auto discarded = samples())
            RING_WARN("Discarding %d samples", discarded);
        av_audio_fifo_free(queue_);
        format_ = format;
        queue_ = av_audio_fifo_alloc(format.sampleFormat, format.nb_channels, frameSize_);
    }
    if (size)
        setFrameSize(size);
}

void
AudioFrameResizer::setFrameSize(int frameSize)
{
    if (frameSize_ != frameSize) {
        frameSize_ = frameSize;
        if (cb_)
            while (auto frame = dequeue())
                cb_(std::move(frame));
    }
}

void
AudioFrameResizer::enqueue(std::shared_ptr<AudioFrame>&& frame)
{
    int ret = 0;
    auto f = frame->pointer();
    AudioFormat format(f->sample_rate, f->channels, (AVSampleFormat)f->format);
    if (format != format_) {
        RING_ERR() << "Expected " << format_ << ", but got " << AudioFormat(f->sample_rate, f->channels, (AVSampleFormat)f->format);
        setFormat(format, frameSize_);
    }

    auto nb_samples = samples();
    if (cb_ && nb_samples == 0 && f->nb_samples == frameSize_) {
        nextOutputPts_ = frame->pointer()->pts + frameSize_;
        cb_(std::move(frame));
        return; // return if frame was just passed through
    }

    // queue reallocates itself if need be
    if ((ret = av_audio_fifo_write(queue_, reinterpret_cast<void**>(f->data), f->nb_samples)) < 0) {
        RING_ERR() << "Audio resizer error: " << libav_utils::getError(ret);
        throw std::runtime_error("Failed to add audio to frame resizer");
    }

    if (nextOutputPts_ == 0)
        nextOutputPts_ = frame->pointer()->pts - nb_samples;

    if (cb_)
        while (auto frame = dequeue())
            cb_(std::move(frame));
}

std::shared_ptr<AudioFrame>
AudioFrameResizer::dequeue()
{
    if (samples() < frameSize_)
        return {};

    auto frame = std::make_unique<AudioFrame>(format_, frameSize_);
    int ret;
    if ((ret = av_audio_fifo_read(queue_, reinterpret_cast<void**>(frame->pointer()->data), frameSize_)) < 0) {
        RING_ERR() << "Could not read samples from queue: " << libav_utils::getError(ret);
        return {};
    }
    frame->pointer()->pts = nextOutputPts_;
    nextOutputPts_ += frameSize_;
    return frame;
}

} // namespace ring
