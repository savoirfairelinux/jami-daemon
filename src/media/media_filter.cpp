/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "logger.h"
#include "media_filter.h"
#include "media_buffer.h"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include <algorithm>
#include <functional>
#include <memory>
#include <sstream>
#include <thread>

namespace jami {

MediaFilter::MediaFilter() {}

MediaFilter::~MediaFilter()
{
    clean();
}

std::string
MediaFilter::getFilterDesc() const
{
    return desc_;
}

int
MediaFilter::initialize(const std::string& filterDesc, const std::vector<MediaStream>& msps)
{
    int ret = 0;
    desc_ = filterDesc;
    graph_ = avfilter_graph_alloc();

    if (!graph_)
        return fail("Failed to allocate filter graph", AVERROR(ENOMEM));

    graph_->nb_threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 2));

    AVFilterInOut* in;
    AVFilterInOut* out;
    if ((ret = avfilter_graph_parse2(graph_, desc_.c_str(), &in, &out)) < 0)
        return fail("Failed to parse filter graph", ret);

    using AVFilterInOutPtr = std::unique_ptr<AVFilterInOut, std::function<void(AVFilterInOut*)>>;
    AVFilterInOutPtr outputs(out, [](AVFilterInOut* f) { avfilter_inout_free(&f); });
    AVFilterInOutPtr inputs(in, [](AVFilterInOut* f) { avfilter_inout_free(&f); });

    if (outputs && outputs->next)
        return fail("Filters with multiple outputs are not supported", AVERROR(ENOTSUP));

    if ((ret = initOutputFilter(outputs.get())) < 0)
        return fail("Failed to create output for filter graph", ret);

    // make sure inputs linked list is the same size as msps
    size_t count = 0;
    AVFilterInOut* dummyInput = inputs.get();
    while (dummyInput && ++count) // increment count before evaluating its value
        dummyInput = dummyInput->next;
    if (count != msps.size())
        return fail(
            "Size mismatch between number of inputs in filter graph and input parameter array",
            AVERROR(EINVAL));

    for (AVFilterInOut* current = inputs.get(); current; current = current->next) {
        if (!current->name)
            return fail("Filters require non empty names", AVERROR(EINVAL));
        std::string_view name = current->name;
        auto it = std::find_if(msps.begin(), msps.end(), [name](const MediaStream& msp) {
            return msp.name == name;
        });
        if (it != msps.end()) {
            if ((ret = initInputFilter(current, *it)) < 0) {
                return fail(fmt::format("Failed to initialize input: {}", name), ret);
            }
        } else {
            return fail(fmt::format("Failed to find matching parameters for: {}", name), ret);
        }
    }

    if ((ret = avfilter_graph_config(graph_, nullptr)) < 0)
        return fail("Failed to configure filter graph", ret);

    JAMI_DBG() << "Filter graph initialized with: " << desc_;
    initialized_ = true;
    return 0;
}

const MediaStream&
MediaFilter::getInputParams(const std::string& inputName) const
{
    for (const auto& ms : inputParams_)
        if (ms.name == inputName)
            return ms;
    throw std::out_of_range("Input '" + inputName + "' not found");
}

MediaStream
MediaFilter::getOutputParams() const
{
    MediaStream output;
    if (!output_ || !initialized_) {
        fail("Filter not initialized", -1);
        return output;
    }

    switch (av_buffersink_get_type(output_)) {
    case AVMEDIA_TYPE_VIDEO:
        output.name = "videoOutput";
        output.format = av_buffersink_get_format(output_);
        output.isVideo = true;
        output.timeBase = av_buffersink_get_time_base(output_);
        output.width = av_buffersink_get_w(output_);
        output.height = av_buffersink_get_h(output_);
        output.bitrate = 0;
        output.frameRate = av_buffersink_get_frame_rate(output_);
        break;
    case AVMEDIA_TYPE_AUDIO:
        output.name = "audioOutput";
        output.format = av_buffersink_get_format(output_);
        output.isVideo = false;
        output.timeBase = av_buffersink_get_time_base(output_);
        output.sampleRate = av_buffersink_get_sample_rate(output_);
        output.nbChannels = av_buffersink_get_channels(output_);
        break;
    default:
        output.format = -1;
        break;
    }
    return output;
}

int
MediaFilter::feedInput(AVFrame* frame, const std::string& inputName)
{
    int ret = 0;
    if (!initialized_)
        return fail("Filter not initialized", -1);

    if (!frame)
        return 0;

    for (size_t i = 0; i < inputs_.size(); ++i) {
        auto& ms = inputParams_[i];
        if (ms.name != inputName)
            continue;

        if (ms.format != frame->format
            || (ms.isVideo && (ms.width != frame->width || ms.height != frame->height))
            || (!ms.isVideo
                && (ms.sampleRate != frame->sample_rate || ms.nbChannels != frame->ch_layout.nb_channels))) {
            ms.update(frame);
            if ((ret = reinitialize()) < 0)
                return fail("Failed to reinitialize filter with new input parameters", ret);
        }

        int flags = AV_BUFFERSRC_FLAG_KEEP_REF;
        if ((ret = av_buffersrc_add_frame_flags(inputs_[i], frame, flags)) < 0)
            return fail("Unable to pass frame to filters", ret);
        else
            return 0;
    }
    return fail(fmt::format("Specified filter '{}' not found", inputName), AVERROR(EINVAL));
}

std::unique_ptr<MediaFrame>
MediaFilter::readOutput()
{
    if (!initialized_) {
        fail("Not properly initialized", -1);
        return {};
    }

    std::unique_ptr<MediaFrame> frame;
    switch (av_buffersink_get_type(output_)) {
#ifdef ENABLE_VIDEO
    case AVMEDIA_TYPE_VIDEO:
        frame = std::make_unique<libjami::VideoFrame>();
        break;
#endif
    case AVMEDIA_TYPE_AUDIO:
        frame = std::make_unique<AudioFrame>();
        break;
    default:
        return {};
    }
    auto err = av_buffersink_get_frame(output_, frame->pointer());
    if (err >= 0) {
        return frame;
    } else if (err == AVERROR(EAGAIN)) {
        // no data available right now, try again
    } else if (err == AVERROR_EOF) {
        JAMI_WARN() << "Filters have reached EOF, no more frames will be output";
    } else {
        fail("Error occurred while pulling from filter graph", err);
    }
    return {};
}

void
MediaFilter::flush()
{
    for (size_t i = 0; i < inputs_.size(); ++i) {
        int ret = av_buffersrc_add_frame_flags(inputs_[i], nullptr, 0);
        if (ret < 0) {
            JAMI_ERR() << "Failed to flush filter '" << inputParams_[i].name
                       << "': " << libav_utils::getError(ret);
        }
    }
}

int
MediaFilter::initOutputFilter(AVFilterInOut* out)
{
    int ret = 0;
    const AVFilter* buffersink;
    AVFilterContext* buffersinkCtx = nullptr;
    AVMediaType mediaType = avfilter_pad_get_type(out->filter_ctx->input_pads, out->pad_idx);

    if (mediaType == AVMEDIA_TYPE_VIDEO)
        buffersink = avfilter_get_by_name("buffersink");
    else
        buffersink = avfilter_get_by_name("abuffersink");

    if ((ret
         = avfilter_graph_create_filter(&buffersinkCtx, buffersink, "out", nullptr, nullptr, graph_))
        < 0) {
        avfilter_free(buffersinkCtx);
        return fail("Failed to create buffer sink", ret);
    }

    if ((ret = avfilter_link(out->filter_ctx, out->pad_idx, buffersinkCtx, 0)) < 0) {
        avfilter_free(buffersinkCtx);
        return fail("Unable to link buffer sink to graph", ret);
    }

    output_ = buffersinkCtx;
    return ret;
}

int
MediaFilter::initInputFilter(AVFilterInOut* in, const MediaStream& msp)
{
    int ret = 0;
    AVBufferSrcParameters* params = av_buffersrc_parameters_alloc();
    if (!params)
        return -1;

    const AVFilter* buffersrc;
    AVMediaType mediaType = avfilter_pad_get_type(in->filter_ctx->input_pads, in->pad_idx);
    params->format = msp.format;
    params->time_base = msp.timeBase;
    if (mediaType == AVMEDIA_TYPE_VIDEO) {
        params->width = msp.width;
        params->height = msp.height;
        params->frame_rate = msp.frameRate;
        buffersrc = avfilter_get_by_name("buffer");
    } else {
        params->sample_rate = msp.sampleRate;
        av_channel_layout_default(&params->ch_layout, msp.nbChannels);
        buffersrc = avfilter_get_by_name("abuffer");
    }

    AVFilterContext* buffersrcCtx = nullptr;
    if (buffersrc) {
        char name[128];
        snprintf(name, sizeof(name), "buffersrc_%s_%d", in->name, in->pad_idx);
        buffersrcCtx = avfilter_graph_alloc_filter(graph_, buffersrc, name);
    }
    if (!buffersrcCtx) {
        av_free(params);
        return fail("Failed to allocate filter graph input", AVERROR(ENOMEM));
    }
    ret = av_buffersrc_parameters_set(buffersrcCtx, params);
    av_free(params);
    if (ret < 0)
        return fail("Failed to set filter graph input parameters", ret);

    if ((ret = avfilter_init_str(buffersrcCtx, nullptr)) < 0)
        return fail("Failed to initialize buffer source", ret);

    if ((ret = avfilter_link(buffersrcCtx, 0, in->filter_ctx, in->pad_idx)) < 0)
        return fail("Failed to link buffer source to graph", ret);

    inputs_.push_back(buffersrcCtx);
    inputParams_.emplace_back(msp);
    inputParams_.back().name = in->name;
    return ret;
}

int
MediaFilter::reinitialize()
{
    // keep parameters needed for initialization before clearing filter
    auto params = std::move(inputParams_);
    auto desc = std::move(desc_);
    clean();
    auto ret = initialize(desc, params);
    if (ret >= 0)
        JAMI_DBG() << "Filter graph reinitialized";
    return ret;
}

int
MediaFilter::fail(std::string_view msg, int err) const
{
    if (!msg.empty())
        JAMI_ERR() << msg << ": " << libav_utils::getError(err);
    return err;
}

void
MediaFilter::clean()
{
    initialized_ = false;
    avfilter_graph_free(&graph_); // frees inputs_ and output_
    desc_.clear();
    inputs_.clear();   // don't point to freed memory
    output_ = nullptr; // don't point to freed memory
    inputParams_.clear();
}

} // namespace jami
