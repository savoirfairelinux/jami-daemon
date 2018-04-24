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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "logger.h"
#include "media_filter.h"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include <functional>
#include <memory>
#include <sstream>

namespace ring {

MediaFilter::MediaFilter()
{}

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
MediaFilter::initialize(const std::string filterDesc, MediaFilterParameters mfp)
{
    std::vector<MediaFilterParameters> mfps;
    mfps.push_back(mfp);
    desc_ = filterDesc;
    return initialize(desc_, mfps);
}

int
MediaFilter::initialize(const std::string filterDesc, std::vector<MediaFilterParameters> mfps)
{
    int ret = 0;
    desc_ = filterDesc;
    graph_ = avfilter_graph_alloc();

    if (!graph_)
        return fail("Failed to allocate filter graph", AVERROR(ENOMEM));

    AVFilterInOut* in;
    AVFilterInOut* out;
    if ((ret = avfilter_graph_parse2(graph_, desc_.c_str(), &in, &out)) < 0)
        return fail("Failed to parse filter graph", ret);

    using AVFilterInOutPtr = std::unique_ptr<AVFilterInOut, std::function<void(AVFilterInOut*)>>;
    AVFilterInOutPtr outputs(out, [](AVFilterInOut* f){ avfilter_inout_free(&f); });
    AVFilterInOutPtr inputs(in, [](AVFilterInOut* f){ avfilter_inout_free(&f); });

    if (outputs && outputs->next)
        return fail("Filters with multiple outputs are not supported", AVERROR(ENOTSUP));

    if ((ret = initOutputFilter(outputs.get())) < 0)
        return fail("Failed to create output for filter graph", ret);

    // make sure inputs linked list is the same size as mfps
    size_t count = 0;
    AVFilterInOut* dummyInput = inputs.get();
    while (dummyInput && ++count) // increment count before evaluating its value
        dummyInput = dummyInput->next;
    if (count != mfps.size())
        return fail("Size mismatch between number of inputs in filter graph and input parameter array",
                    AVERROR(EINVAL));

    int index = 0;
    for (AVFilterInOut* current = inputs.get(); current; current = current->next)
        if ((ret = initInputFilter(current, mfps[index++])) < 0)
            return fail("Failed to create input for filter graph", ret);

    if ((ret = avfilter_graph_config(graph_, nullptr)) < 0)
        return fail("Failed to configure filter graph", ret);

    RING_DBG() << "Filter graph initialized with: " << desc_;
    initialized_ = true;
    return 0;
}

int
MediaFilter::feedInput(AVFrame* frame)
{
    int ret = 0;
    if (inputs_.size() == 0)
        return fail("No inputs found", AVERROR(EINVAL));

    auto filterCtx = inputs_[0];
    if (!filterCtx)
        return fail("No inputs found", AVERROR(EINVAL));

    if ((ret = av_buffersrc_write_frame(filterCtx, frame)) < 0)
        return fail("Could not pass frame to filters", ret);
    return 0;
}

int
MediaFilter::feedInput(AVFrame* frame, std::string inputName)
{
    int ret = 0;
    for (size_t i = 0; i < inputs_.size(); ++i) {
        auto filterCtx = inputs_[i];
        int requested = av_buffersrc_get_nb_failed_requests(filterCtx);
        if (requested > 0)
            RING_WARN() << inputNames_[i] << " filter needs more input to produce output";

        if (inputNames_[i] != inputName)
            continue;

        if ((ret = av_buffersrc_write_frame(filterCtx, frame)) < 0)
            return fail("Could not pass frame to filters", ret);
        else
            return 0;
    }

    std::stringstream ss;
    ss << "Specified filter (" << inputName << ") not found";
    return fail(ss.str(), AVERROR(EINVAL));
}

AVFrame*
MediaFilter::readOutput()
{
    int ret = 0;
    AVFrame* frame = av_frame_alloc();
    ret = av_buffersink_get_frame_flags(output_, frame, 0);
    if (ret >= 0) {
        return frame;
    } else if (ret == AVERROR(EAGAIN)) {
        RING_WARN() << "No frame available in sink: " << output_->filter->name
            << " (" << output_->name << "): send more input";
    } else if (ret == AVERROR_EOF) {
        RING_WARN() << "Filters have reached EOF, no more frames will be output";
    } else {
        fail("Error occurred while pulling from filter graph", ret);
    }
    av_frame_free(&frame);
    return NULL;
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

    if ((ret = avfilter_graph_create_filter(&buffersinkCtx, buffersink, "out",
                                            nullptr, nullptr, graph_)) < 0) {
        avfilter_free(buffersinkCtx);
        return fail("Failed to create buffer sink", ret);
    }

    if ((ret = avfilter_link(out->filter_ctx, out->pad_idx, buffersinkCtx, 0)) < 0) {
        avfilter_free(buffersinkCtx);
        return fail("Could not link buffer sink to graph", ret);
    }

    output_ = buffersinkCtx;
    return ret;
}

int
MediaFilter::initInputFilter(AVFilterInOut* in, MediaFilterParameters mfp)
{
    int ret = 0;
    bool simple = !in->name; // simple filters don't require the graph input to be labelled
    AVBufferSrcParameters* params = av_buffersrc_parameters_alloc();
    if (!params)
        return -1;

    const AVFilter* buffersrc;
    AVMediaType mediaType = avfilter_pad_get_type(in->filter_ctx->input_pads, in->pad_idx);
    params->format = mfp.format;
    params->time_base.num = mfp.timeBase.numerator();
    params->time_base.den = mfp.timeBase.denominator();
    if (mediaType == AVMEDIA_TYPE_VIDEO) {
        params->width = mfp.width;
        params->height = mfp.height;
        params->sample_aspect_ratio.num = mfp.aspectRatio.numerator();
        params->sample_aspect_ratio.den = mfp.aspectRatio.denominator();
        params->frame_rate.num = mfp.frameRate.numerator();
        params->frame_rate.den = mfp.frameRate.denominator();
        buffersrc = avfilter_get_by_name("buffer");
    } else {
        params->sample_rate = mfp.sampleRate;
        params->channel_layout = av_get_default_channel_layout(mfp.nbChannels);
        buffersrc = avfilter_get_by_name("abuffer");
    }

    AVFilterContext* buffersrcCtx = nullptr;
    if (buffersrc) {
        char name[128];
        if (simple)
            snprintf(name, sizeof(name), "buffersrc");
        else
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
    if (simple)
        inputNames_.push_back("default");
    else
        inputNames_.push_back(in->name);
    return ret;
}

int
MediaFilter::fail(std::string msg, int err)
{
    if (!msg.empty())
        RING_ERR() << msg << ": " << libav_utils::getError(err);
    failed_ = true;
    //clean();
    return err;
}

void
MediaFilter::clean()
{
    avfilter_graph_free(&graph_);
}

} // namespace ring
