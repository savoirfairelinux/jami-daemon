/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "audio_format.h"
#include "media/media_buffer.h"
#include "noncopyable.h"

extern "C" {
struct AVFrame;
struct SwrContext;
}

namespace jami {

class AudioBuffer;

/**
 * @brief Wrapper class for libswresample
 */
class Resampler
{
public:
    Resampler();
    ~Resampler();

    /**
     * @brief Resample a frame.
     *
     * Resample from @input format to @output format.
     *
     * NOTE: sample_rate, ch_layout, and format should be set on @output
     */
    int resample(const AVFrame* input, AVFrame* output);

    /**
     * @brief Wrapper around resample(AVFrame*, AVFrame*) for convenience.
     */
    void resample(const AudioBuffer& dataIn, AudioBuffer& dataOut);

    /**
     * @brief Wrapper around resample(AVFrame*, AVFrame*) for convenience.
     */
    std::unique_ptr<AudioFrame> resample(std::unique_ptr<AudioFrame>&& in, const AudioFormat& out);

    /**
     * @brief Wrapper around resample(AVFrame*, AVFrame*) for convenience.
     */
    std::shared_ptr<AudioFrame> resample(std::shared_ptr<AudioFrame>&& in, const AudioFormat& out);

private:
    NON_COPYABLE(Resampler);

    /**
     * @brief Reinitializes filter according to new format.
     *
     * Reinitializes the resampler when new settings are detected. As long as both input and
     * output formats don't change, this will only be called once.
     */
    void reinit(const AVFrame* in, const AVFrame* out);

    /**
     * @brief Libswresample resampler context.
     *
     * NOTE SwrContext is an imcomplete type and cannot be stored in a smart pointer.
     */
    SwrContext* swrCtx_;

    /**
     * @brief Number of times @swrCtx_ has been initialized with no successful audio resampling.
     *
     * 0: Uninitialized
     * 1: Initialized
     * >1: Invalid frames or formats, reinit is going to be called in an infinite loop
     */
    unsigned initCount_;
};

} // namespace jami
