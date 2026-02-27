/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "libav_deps.h"
#include "logger.h"
#include <libavutil/frame.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include "resampler.h"
#include "libav_utils.h"

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
    auto* swrCtx = swr_alloc();
    if (!swrCtx) {
        JAMI_ERROR("[{}] Unable to allocate resampler context", fmt::ptr(this));
        throw std::bad_alloc();
    }

    int ret = av_opt_set_chlayout(swrCtx, "ichl", &in->ch_layout, 0);
    if (ret < 0) {
        swr_free(&swrCtx);
        char layout_buf[64];
        av_channel_layout_describe(&in->ch_layout, layout_buf, sizeof(layout_buf));
        JAMI_ERROR("[{}] Failed to set input channel layout {}: {}",
                   fmt::ptr(this),
                   layout_buf,
                   libav_utils::getError(ret));
        throw std::runtime_error("Failed to set input channel layout");
    }
    ret = av_opt_set_int(swrCtx, "isr", in->sample_rate, 0);
    if (ret < 0) {
        swr_free(&swrCtx);
        JAMI_ERROR("[{}] Failed to set input sample rate {}: {}",
                   fmt::ptr(this),
                   in->sample_rate,
                   libav_utils::getError(ret));
        throw std::runtime_error("Failed to set input sample rate");
    }
    ret = av_opt_set_sample_fmt(swrCtx, "isf", static_cast<AVSampleFormat>(in->format), 0);
    if (ret < 0) {
        swr_free(&swrCtx);
        JAMI_ERROR("[{}] Failed to set input sample format {}: {}",
                   fmt::ptr(this),
                   av_get_sample_fmt_name(static_cast<AVSampleFormat>(in->format)),
                   libav_utils::getError(ret));
        throw std::runtime_error("Failed to set input sample format");
    }

    ret = av_opt_set_chlayout(swrCtx, "ochl", &out->ch_layout, 0);
    if (ret < 0) {
        swr_free(&swrCtx);
        char layout_buf[64];
        av_channel_layout_describe(&out->ch_layout, layout_buf, sizeof(layout_buf));
        JAMI_ERROR("[{}] Failed to set output channel layout {}: {}",
                   fmt::ptr(this),
                   layout_buf,
                   libav_utils::getError(ret));
        throw std::runtime_error("Failed to set output channel layout");
    }
    ret = av_opt_set_int(swrCtx, "osr", out->sample_rate, 0);
    if (ret < 0) {
        swr_free(&swrCtx);
        JAMI_ERROR("[{}] Failed to set output sample rate {}: {}",
                   fmt::ptr(this),
                   out->sample_rate,
                   libav_utils::getError(ret));
        throw std::runtime_error("Failed to set output sample rate");
    }
    ret = av_opt_set_sample_fmt(swrCtx, "osf", static_cast<AVSampleFormat>(out->format), 0);
    if (ret < 0) {
        swr_free(&swrCtx);
        JAMI_ERROR("[{}] Failed to set output sample format {}: {}",
                   fmt::ptr(this),
                   av_get_sample_fmt_name(static_cast<AVSampleFormat>(out->format)),
                   libav_utils::getError(ret));
        throw std::runtime_error("Failed to set output sample format");
    }

    /**
     * Downmixing from 5.1 requires extra setup, since libswresample is unable to do it
     * automatically (not yet implemented).
     *
     * Source: https://www.atsc.org/wp-content/uploads/2015/03/A52-201212-17.pdf
     * Section 7.8.2 for the algorithm
     * Tables 5.9 and 5.10 for the coefficients clev and slev
     *
     * LFE downmixing is optional, so any coefficient can be used, we use +6dB for mono and
     * +0dB in each channel for stereo.
     */
    if (in->ch_layout.u.mask == AV_CH_LAYOUT_5POINT1 || in->ch_layout.u.mask == AV_CH_LAYOUT_5POINT1_BACK) {
        int ret = 0;
        // NOTE: MSVC is unable to allocate dynamic size arrays on the stack
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
            ret = swr_set_matrix(swrCtx, matrix[0], 6);
        } else {
            double matrix[1][6];
            // M = 1.0*FL + 1.414*FC + 1.0*FR + 0.707*BL + 0.707*BR + 2.0*LFE
            matrix[0][0] = 1;
            matrix[0][1] = 1;
            matrix[0][2] = 1.414;
            matrix[0][3] = 2;
            matrix[0][4] = 0.707;
            matrix[0][5] = 0.707;
            ret = swr_set_matrix(swrCtx, matrix[0], 6);
        }
        if (ret < 0) {
            swr_free(&swrCtx);
            JAMI_ERROR("[{}]  Failed to set mixing matrix: {}", fmt::ptr(this), libav_utils::getError(ret));
            throw std::runtime_error("Failed to set mixing matrix");
        }
    }

    ret = swr_init(swrCtx);
    if (ret >= 0) {
        std::swap(swrCtx_, swrCtx);
        swr_free(&swrCtx);
        JAMI_DEBUG("[{}] Succesfully (re)initialized resampler context from {} to {}",
                   fmt::ptr(this),
                   libav_utils::getFormat(in).toString(),
                   libav_utils::getFormat(out).toString());
        ++initCount_;
    } else {
        swr_free(&swrCtx);
        JAMI_ERROR("[{}] Runtime error: Failed to initialize resampler context: {}",
                   fmt::ptr(this),
                   libav_utils::getError(ret));
        throw std::runtime_error("Failed to initialize resampler context");
    }
}

int
Resampler::resample(const AVFrame* input, AVFrame* output)
{
    bool firstFrame = (initCount_ == 0);
    if (!initCount_)
        reinit(input, output);

    int ret = swr_convert_frame(swrCtx_, output, input);
    if (ret & AVERROR_INPUT_CHANGED || ret & AVERROR_OUTPUT_CHANGED) {
        // Under certain conditions, the resampler reinits itself in an infinite loop. This is
        // indicative of an underlying problem in the code. This check is so the backtrace
        // doesn't get mangled with a bunch of calls to Resampler::resample
        if (initCount_ > 1) {
            // JAMI_ERROR("Infinite loop detected in audio resampler, please open an issue on https://git.jami.net");
            JAMI_ERROR("[{}] Loop detected in audio resampler when resampling from {} to {}",
                       fmt::ptr(this),
                       libav_utils::getFormat(input).toString(),
                       libav_utils::getFormat(output).toString());
            throw std::runtime_error("Infinite loop detected in audio resampler");
        }
        reinit(input, output);
        return resample(input, output);
    }

    if (ret < 0) {
        JAMI_ERROR("[{}] Failed to resample frame: {}", fmt::ptr(this), libav_utils::getError(ret));
        return -1;
    }

    if (firstFrame) {
        // we just resampled the first frame
        auto targetOutputLength = av_rescale_rnd(input->nb_samples,
                                                 output->sample_rate,
                                                 input->sample_rate,
                                                 AV_ROUND_UP);
        if (output->nb_samples < targetOutputLength) {
            // create new frame with more samples, padded with silence
            JAMI_WARNING("[{}] Adding {} samples of silence at beginning of first frame to reach {} samples",
                         fmt::ptr(this),
                         targetOutputLength - output->nb_samples,
                         targetOutputLength);
            auto* newOutput = av_frame_alloc();
            if (!newOutput) {
                JAMI_ERROR("[{}] Failed to clone output frame for resizing", fmt::ptr(this));
                return -1;
            }
            av_frame_copy_props(newOutput, output);
            newOutput->format = output->format;
            newOutput->nb_samples = static_cast<int>(targetOutputLength);
            newOutput->ch_layout = output->ch_layout;
            newOutput->channel_layout = output->channel_layout;
            newOutput->sample_rate = output->sample_rate;
            int bufferRet = av_frame_get_buffer(newOutput, 0);
            if (bufferRet < 0) {
                JAMI_ERROR("[{}] Failed to allocate new output frame buffer: {}",
                           fmt::ptr(this),
                           libav_utils::getError(bufferRet));
                av_frame_free(&newOutput);
                return -1;
            }
            auto sampleOffset = targetOutputLength - output->nb_samples;
            bufferRet = av_samples_set_silence(newOutput->data,
                                               0,
                                               static_cast<int>(sampleOffset),
                                               output->ch_layout.nb_channels,
                                               static_cast<AVSampleFormat>(output->format));
            if (bufferRet < 0) {
                JAMI_ERROR("[{}] Failed to set silence on new output frame: {}",
                           fmt::ptr(this),
                           libav_utils::getError(bufferRet));
                av_frame_free(&newOutput);
                return -1;
            }
            // copy old data to new frame at offset sampleOffset
            bufferRet = av_samples_copy(newOutput->data,
                                        output->data,
                                        static_cast<int>(sampleOffset),
                                        0,
                                        output->nb_samples,
                                        output->ch_layout.nb_channels,
                                        static_cast<AVSampleFormat>(output->format));
            if (bufferRet < 0) {
                JAMI_ERROR("[{}] Failed to copy data to new output frame: {}",
                           fmt::ptr(this),
                           libav_utils::getError(bufferRet));
                av_frame_free(&newOutput);
                return -1;
            }
            JAMI_DEBUG("[{}] Resampled first frame. Resized from {} to {} samples",
                       fmt::ptr(this),
                       output->nb_samples,
                       newOutput->nb_samples);
            // replace output frame buffer
            av_frame_unref(output);
            av_frame_move_ref(output, newOutput);
            av_frame_free(&newOutput);
        }
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
    auto* inPtr = in->pointer();
    if (inPtr == nullptr) {
        return {};
    }

    if (inPtr->sample_rate == (int) format.sample_rate && inPtr->ch_layout.nb_channels == (int) format.nb_channels
        && (AVSampleFormat) inPtr->format == format.sampleFormat) {
        return std::move(in);
    }

    auto output = std::make_shared<AudioFrame>(format);
    if (auto* outPtr = output->pointer()) {
        resample(inPtr, outPtr);
        output->has_voice = in->has_voice;
        return output;
    }
    return {};
}

} // namespace jami
