/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *  Inspired by ringbuffer of Audacity Project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audioloop.h"
#include "logger.h"
#include "libav_deps.h"

#include <algorithm> // std::min

namespace jami {

AudioLoop::AudioLoop(AudioFormat format)
    : format_(format)
    , buffer_(av_frame_alloc())
    , pos_(0)
{
}

AudioLoop::~AudioLoop()
{}

void
AudioLoop::seek(double relative_position)
{
    pos_ = static_cast<double>(getSize() * relative_position * 0.01);
}

void
AudioLoop::getNext(AVFrame* output, bool mute)
{
    if (!buffer_) {
        JAMI_ERR("buffer is NULL");
        return;
    }

    const size_t buf_samples = buffer_->nb_samples;
    size_t pos = pos_;
    size_t total_samples = output->nb_samples;
    size_t output_pos = 0;

    if (buf_samples == 0) {
        JAMI_ERR("Audio loop size is 0");
        return;
    } else if (pos >= buf_samples) {
        JAMI_ERR("Invalid loop position %zu", pos);
        return;
    }

    while (total_samples != 0) {
        size_t samples = std::min(total_samples, buf_samples - pos);
        if (not mute)
            av_samples_copy(output->data, buffer_->data, output_pos, pos, samples, format_.nb_channels, format_.sampleFormat);
        else
            av_samples_set_silence(output->data, output_pos, samples, format_.nb_channels, format_.sampleFormat);
        output_pos += samples;
        pos = (pos + samples) % buf_samples;
        total_samples -= samples;
    }

    pos_ = pos;
    onBufferFinish();
}

void
AudioLoop::onBufferFinish()
{}

std::unique_ptr<AudioFrame>
AudioLoop::getNext(size_t samples, bool mute)
{
    if (samples == 0) {
        samples = buffer_->sample_rate / 50;
    }
    auto buffer = std::make_unique<AudioFrame>(format_, samples);
    getNext(buffer->pointer(), mute);
    return buffer;
}

} // namespace jami
