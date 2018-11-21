/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
#include "noncopyable.h"
#include "ring_types.h"

struct AVFrame;
struct SwrContext;

namespace ring {

/**
 * Wrapper class for libswresample
 */
class Resampler {
    public:
        Resampler();
        ~Resampler();

        /**
         * Resample from @input format to @output format.
         * NOTE: sample_rate, channel_layout, and format should be set on @output
         */
        int resample(const AVFrame* input, AVFrame* output);

        /**
         * Resample from @dataIn format to @dataOut format.
         *
         * NOTE: This is a wrapper for resample(AVFrame*, AVFrame*)
         */
        void resample(const AudioBuffer& dataIn, AudioBuffer& dataOut);

        std::unique_ptr<AudioFrame> resample(const AudioFrame& in, const AudioFormat& out) {
            auto output = std::make_unique<AudioFrame>(out);
            resample(in.pointer(), output->pointer());
            return output;
        }

    private:
        NON_COPYABLE(Resampler);

        /**
         * Reinitializes the resampler when new settings are detected. As long as both input and
         * output buffers always have the same formats, will never be called, as the first
         * initialization is done in swr_convert_frame.
         */
        void reinit(const AudioFormat& in, const AudioFormat& out);

        SwrContext* swrCtx_; // incomplete type, cannot be a unique_ptr
        bool initialized_;
};

} // namespace ring
