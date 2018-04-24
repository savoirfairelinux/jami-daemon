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
MediaFilter::initialize(const std::string filterDesc, AVCodecContext* c)
{
    int ret = 0;
    AVFilterInOut* outputs = nullptr;
    AVFilterInOut* inputs = nullptr;
    desc_ = filterDesc;
    graph_ = avfilter_graph_alloc();

    if (!graph_) {
        return fail("Failed to allocate filter graph", AVERROR(ENOMEM));
    }

    if ((ret = avfilter_graph_parse2(graph_, filterDesc.c_str(), &inputs, &outputs)) < 0) {
        return fail("Failed to parse filter graph", ret);
    }

    for (AVFilterInOut* current = outputs; current; current = current->next) {
        if ((ret = initOutputFilter(current)) < 0) {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            return fail("Failed to create output for filter graph", ret);
        }
    }

    for (AVFilterInOut* current = inputs; current; current = current->next) {
        if ((ret = initInputFilter(current, c)) < 0) {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            return fail("Failed to create input for filter graph", ret);
        }
    }

    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);

    if ((ret = avfilter_graph_config(graph_, nullptr)) < 0) {
        return fail("Failed to configure filter graph", ret);
    }

    RING_DBG() << "Filter graph initialized";
    initialized_ = true;
    return 0;
}

int
MediaFilter::feedInput(AVFrame* frame)
{
    int ret = 0;
    for (auto filterCtx : inputs_) {
        int requested = av_buffersrc_get_nb_failed_requests(filterCtx);
        if (requested > 0) {
            // TODO peek input or reinit
        }

        // TODO send to correct filter ctx
        if ((ret = av_buffersrc_add_frame(filterCtx, frame)) < 0) {
            return fail("Could not pass frame to filters", ret);
        }
    }
    return 0;
}

// TODO only allow 1 output?
std::vector<AVFrame>
MediaFilter::readOutput()
{
    int ret = 0;
    std::vector<AVFrame> frames;
    for (auto filterCtx : outputs_) {
        AVFrame* frame = av_frame_alloc();
        ret = av_buffersink_get_frame_flags(filterCtx, frame, 0);
        if (ret >= 0) {
            frames.push_back(*frame);
            continue;
        } else if (ret == AVERROR(EAGAIN)) {
            RING_WARN() << "No frame available in sink: " << filterCtx->name
                << "[" << filterCtx->filter->name << "]: send more input";
        } else if (ret == AVERROR_EOF) {
            RING_WARN() << "Filters have reached EOF, no more frames will be output";
        } else {
            fail("Error occurred while pulling from filter graph", ret);
        }
        av_frame_free(&frame);
    }
    return frames;
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

    outputs_.push_back(buffersinkCtx);
    return ret;
}

int
MediaFilter::initInputFilter(AVFilterInOut* in, AVCodecContext* c)
{
    int ret = 0;
    AVBufferSrcParameters* params = av_buffersrc_parameters_alloc();
    if (!params)
        return -1;

    const AVFilter* buffersrc;
    AVMediaType mediaType = avfilter_pad_get_type(in->filter_ctx->input_pads, in->pad_idx);
    if (mediaType == AVMEDIA_TYPE_VIDEO) {
        params->format = c->pix_fmt;
        params->time_base = c->time_base;
        params->width = c->width;
        params->height = c->height;
        params->sample_aspect_ratio = c->sample_aspect_ratio;
        params->frame_rate = c->framerate;
        buffersrc = avfilter_get_by_name("buffer");
    } else {
        params->format = c->sample_fmt;
        params->time_base = c->time_base;
        params->sample_rate = c->sample_rate;
        params->channel_layout = c->channel_layout;
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
        return fail("", -1);
    }
    ret = av_buffersrc_parameters_set(buffersrcCtx, params);
    av_free(params);
    if (ret < 0)
        return fail("", -1);

    if ((ret = avfilter_init_str(buffersrcCtx, nullptr)) < 0)
        return fail("Failed to initialize buffer source", ret);

    if ((ret = avfilter_link(buffersrcCtx, 0, in->filter_ctx, in->pad_idx)) < 0)
        return fail("Failed to link buffer source to graph", ret);

    inputs_.push_back(buffersrcCtx);
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
