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

#include "libav_deps.h"
#include "logger.h"
#include "resampler.h"

extern "C" {
#include <libswresample/swresample.h>
}

namespace jami {

Resampler::Resampler()
    : swrCtx_(swr_alloc())
    , initCount_(0)
{}

Resampler::~Resampler()
{
    swr_free(&swrCtx_);
}

void
Resampler::reinit(const AVFrame* in, const AVFrame* out)
{
    // NOTE swr_set_matrix should be called on an uninitialized context
    auto swrCtx = swr_alloc();
    if (!swrCtx) {
        JAMI_ERR() << "Cannot allocate resampler context";
        throw std::bad_alloc();
    }

    av_opt_set_chlayout(swrCtx, "ichl", &in->ch_layout, 0);
    av_opt_set_int(swrCtx, "isr", in->sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx, "isf", static_cast<AVSampleFormat>(in->format), 0);

    av_opt_set_chlayout(swrCtx, "ochl", &out->ch_layout, 0);
    av_opt_set_int(swrCtx, "osr", out->sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx, "osf", static_cast<AVSampleFormat>(out->format), 0);

    /**
     * Downmixing from 5.1 requires extra setup, since libswresample can't do it automatically
     * (not yet implemented).
     *
     * Source: https://www.atsc.org/wp-content/uploads/2015/03/A52-201212-17.pdf
     * Section 7.8.2 for the algorithm
     * Tables 5.9 and 5.10 for the coefficients clev and slev
     *
     * LFE downmixing is optional, so any coefficient can be used, we use +6dB for mono and
     * +0dB in each channel for stereo.
     */
    if (in->ch_layout.u.mask == AV_CH_LAYOUT_5POINT1
        || in->ch_layout.u.mask == AV_CH_LAYOUT_5POINT1_BACK) {
        // NOTE MSVC can't allocate dynamic size arrays on the stack
        if (out->ch_layout.nb_channels == 2) {
            double matrix[2][6];
            // L = 1.0*FL + 0.707*FC + 0.707*BL + 1.0*LFE
            matrix[0][0] = 1;
            matrix[0][1] = 0;
            matrix[0][2] = 0.707;
            matrix[0][3] = 1;
            matrix[0][4] = 0.707;
            matrix[0][5] = 0;
            // R = 1.0*FR + 0.707*FC + 0.707*BR + 1.0*LFE
            matrix[1][0] = 0;
            matrix[1][1] = 1;
            matrix[1][2] = 0.707;
            matrix[1][3] = 1;
            matrix[1][4] = 0;
            matrix[1][5] = 0.707;
            swr_set_matrix(swrCtx, matrix[0], 6);
        } else {
            double matrix[1][6];
            // M = 1.0*FL + 1.414*FC + 1.0*FR + 0.707*BL + 0.707*BR + 2.0*LFE
            matrix[0][0] = 1;
            matrix[0][1] = 1;
            matrix[0][2] = 1.414;
            matrix[0][3] = 2;
            matrix[0][4] = 0.707;
            matrix[0][5] = 0.707;
            swr_set_matrix(swrCtx, matrix[0], 6);
        }
    }

    if (swr_init(swrCtx) >= 0) {
        std::swap(swrCtx_, swrCtx);
        swr_free(&swrCtx);
        ++initCount_;
    } else {
        std::string msg = "Failed to initialize resampler context";
        JAMI_ERR() << msg;
        throw std::runtime_error(msg);
    }
}

int
Resampler::resample(const AVFrame* input, AVFrame* output)
{
    if (!initCount_)
        reinit(input, output);

    int ret = swr_convert_frame(swrCtx_, output, input);
    if (ret & AVERROR_INPUT_CHANGED || ret & AVERROR_OUTPUT_CHANGED) {
        // Under certain conditions, the resampler reinits itself in an infinite loop. This is
        // indicative of an underlying problem in the code. This check is so the backtrace
        // doesn't get mangled with a bunch of calls to Resampler::resample
        if (initCount_ > 1) {
            JAMI_ERROR("Infinite loop detected in audio resampler, please open an issue on https://git.jami.net");
            throw std::runtime_error("Resampler");
        }
        reinit(input, output);
        return resample(input, output);
    } else if (ret < 0) {
        JAMI_ERROR("Failed to resample frame");
        return -1;
    }

    // Resampling worked, reset count to 1 so reinit isn't called again
    initCount_ = 1;
    return 0;
}

std::unique_ptr<AudioFrame>
Resampler::resample(std::unique_ptr<AudioFrame>&& in, const AudioFormat& format)
{
    if (in->pointer()->sample_rate == (int) format.sample_rate
        && in->pointer()->ch_layout.nb_channels == (int) format.nb_channels
        && (AVSampleFormat) in->pointer()->format == format.sampleFormat) {
        return std::move(in);
    }
    auto output = std::make_unique<AudioFrame>(format);
    resample(in->pointer(), output->pointer());
    output->has_voice = in->has_voice;
    return output;
}

std::shared_ptr<AudioFrame>
Resampler::resample(std::shared_ptr<AudioFrame>&& in, const AudioFormat& format)
{
    if (not in) {
        return {};
    }
    auto inPtr = in->pointer();
    if (inPtr == nullptr) {
        return {};
    }

    if (inPtr->sample_rate == (int) format.sample_rate
        && inPtr->ch_layout.nb_channels == (int) format.nb_channels
        && (AVSampleFormat) inPtr->format == format.sampleFormat) {
        return std::move(in);
    }

    auto output = std::make_shared<AudioFrame>(format);
    if (auto outPtr = output->pointer()) {
        resample(inPtr, outPtr);
        output->has_voice = in->has_voice;
        return output;
    }
    return {};
}

} // namespace jami
