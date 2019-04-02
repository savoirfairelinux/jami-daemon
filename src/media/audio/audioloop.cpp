/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#include <algorithm> // std::min

namespace jami {

AudioLoop::AudioLoop(unsigned int sampleRate) :
    buffer_(new AudioBuffer(0, AudioFormat(sampleRate, 1))), pos_(0)
{}

AudioLoop::~AudioLoop()
{
    delete buffer_;
}

void
AudioLoop::seek(double relative_position)
{
    pos_ = static_cast<double>(buffer_->frames() * relative_position * 0.01);
}

void
AudioLoop::getNext(AudioBuffer& output, double gain)
{
    if (!buffer_) {
        JAMI_ERR("buffer is NULL");
        return;
    }

    const size_t buf_samples = buffer_->frames();
    size_t pos = pos_;
    size_t total_samples = output.frames();
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
        output.copy(*buffer_, samples, pos, output_pos);
        output_pos += samples;
        pos = (pos + samples) % buf_samples;
        total_samples -= samples;
    }

    output.applyGain(gain);
    pos_ = pos;
    onBufferFinish();
}

void AudioLoop::onBufferFinish() {}

std::unique_ptr<AudioFrame>
AudioLoop::getNext(size_t samples)
{
    if (samples == 0) {
        samples = buffer_->getSampleRate() / 50;
    }
    AudioBuffer buff(samples, buffer_->getFormat());
    getNext(buff, 1);
    return buff.toAVFrame();
}

} // namespace jami
