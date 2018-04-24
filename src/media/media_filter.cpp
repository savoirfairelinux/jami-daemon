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

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

namespace ring {

MediaFilter::MediaFilter()
{}

MediaFilter::~MediaFilter()
{
    clean();
}

std::string
MediaFilter::getFilterChain() const
{
    return filterChain_;
}

int
MediaFilter::initializeFilters(AVCodecContext* codecCtx, std::string& filterChain)
{
    clean();

    int ret = 0;
    outputs_ = avfilter_inout_alloc();
    inputs_ = avfilter_inout_alloc();
    graph_ = avfilter_graph_alloc();

    if (!outputs_ || !inputs_ || !graph_) {
        RING_ERR() << "Failed to allocate filter chain";
        clean();
        return -1;
    }

    filterChain_ = filterChain;
    isVideo_ = (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO);

    std::stringstream args;
    std::string srcName, sinkName;
    if (isVideo_) {
        srcName = "buffer";
        sinkName = "buffersink";
        args << "video_size=" << codecCtx->width << "x" << codecCtx->height
            << ":pix_fmt=" << codecCtx->pix_fmt
            << ":time_base=" << codecCtx->time_base.num << "/" << codecCtx->time_base.den
            << ":pixel_aspect=" << codecCtx->sample_aspect_ratio.num
            << "/" << codecCtx->sample_aspect_ratio.den;
    } else {
        srcName = "abuffer";
        sinkName = "abuffersink";
        args << ":time_base=" << codecCtx->time_base.num << "/" << codecCtx->time_base.den
            << ":sample_rate=" << codecCtx->sample_rate
            << ":sample_fmt=" << av_get_sample_fmt_name(codecCtx->sample_fmt)
            << ":channel_layout=" << codecCtx->channel_layout;
    }

    const AVFilter* buffersrc = avfilter_get_by_name(srcName.c_str());
    const AVFilter* buffersink = avfilter_get_by_name(sinkName.c_str());

    if ((ret = avfilter_graph_create_filter(&srcCtx_, buffersrc, "in",
            args.str().c_str(), nullptr, graph_)) < 0) {
        RING_ERR() << "Failed to create buffer source filter";
        clean();
        return -1;
    }

    if ((ret = avfilter_graph_create_filter(&sinkCtx_, buffersink, "out",
            nullptr, nullptr, graph_)) < 0) {
        RING_ERR() << "Failed to create buffer sink filter";
        clean();
        return -1;
    }

    // TODO av_opt_set pix_fmts, sample_fmts, channel_layouts, sample_rates

    //outputs_->name = av_strdup("in"); // default label
    outputs_->filter_ctx = srcCtx_;
    outputs_->pad_idx = 0;
    outputs_->next = nullptr;

    //inputs_->name = av_strdup("out"); // default label
    inputs_->filter_ctx = srcCtx_;
    inputs_->pad_idx = 0;
    inputs_->next = nullptr;

    if ((ret = avfilter_graph_parse_ptr(graph_, filterChain_.c_str(), &inputs_, &outputs_, nullptr)) < 0) {
        RING_ERR() << "Could not parse filter chain: " << filterChain_;
        clean();
        return -1;
    }

    if ((ret = avfilter_graph_config(graph_, nullptr)) < 0) {
        RING_ERR() << "Failed to configure filter chain";
        clean();
        return -1;
    }

    avfilter_inout_free(&outputs_);
    avfilter_inout_free(&inputs_);

    return ret;
}

int
MediaFilter::applyFilters(AVFrame* frame)
{
    int ret = 0;
    AVFrame* filtered = nullptr;

    if ((ret = av_buffersrc_add_frame_flags(srcCtx_, frame, 0)) < 0) {
        RING_ERR() << "Error feeding filter chain";
        return ret;
    }

    while (ret >= 0) {
        filtered = av_frame_alloc();
        if (!filtered) {
            ret = -1;
            break;
        }

        if ((ret = av_buffersink_get_frame(sinkCtx_, filtered)) < 0) {
            // not an error
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;

            av_frame_free(&filtered);
            break;
        }

        // move filtered frame into frame, so caller can access it
        av_frame_unref(frame);
        av_frame_move_ref(frame, filtered);
        av_frame_free(&filtered);
    }

    return ret;
}

void
MediaFilter::clean()
{
    avfilter_inout_free(&inputs_);
    avfilter_inout_free(&outputs_);
    avfilter_graph_free(&graph_);
}

} // namespace ring
