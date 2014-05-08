/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "libav_deps.h"
#include "video_filter.h"
#include "check.h"
#include "logger.h"
#include <assert.h>

namespace sfl_video {

VideoFilter::VideoFilter() :
    filterGraph_(nullptr)
    , vbufferCtx_(nullptr)
    , vnegateCtx_(nullptr)
    , vbufferSinkCtx_(nullptr)
{
    filterGraph_ = avfilter_graph_alloc();
    if (!filterGraph_) {
        ERROR("unable to create filter graph: out of memory");
        return;
    }

    AVFilter* vbufferFilter = avfilter_get_by_name("buffer");
    AVFilter* vnegateFilter = avfilter_get_by_name("negate");
    AVFilter* vbufferSinkFilter = avfilter_get_by_name("buffersink");
    assert(vbufferFilter && vnegateFilter && vbufferSinkFilter);

    int err;

    err = avfilter_graph_create_filter(&vbufferCtx_, vbufferFilter, NULL,
                                       "video_size=352x288:pix_fmt=yuv420p:frame_rate=1/30",
                                       NULL, filterGraph_);
    assert(err >= 0); /* vbufferFilter */

    err = avfilter_graph_create_filter(&vnegateCtx_, vnegateFilter, NULL, NULL,
                                       NULL, filterGraph_);
    assert(err >= 0); /* vnegateFilter */

    err = avfilter_graph_create_filter(&vbufferSinkCtx_, vbufferSinkFilter, NULL,
                                       NULL, NULL, filterGraph_);
    assert(err >= 0); /* vbufferSinkFilter */

    if (err >= 0) err = avfilter_link(vbufferCtx_, 0, vnegateCtx_, 0);
    if (err >= 0) err = avfilter_link(vnegateCtx_, 0, vbufferSinkCtx_, 0);
    if (err < 0) {
        ERROR("error connecting filters");
        return;
    }
    err = avfilter_graph_config(filterGraph_, NULL);
    if (err < 0) {
        ERROR("error configuring the filter graph");
        return;
    }
}

VideoFilter::~VideoFilter()
{
    avfilter_graph_free(&filterGraph_);
}

void VideoFilter::process(VideoFrame& input, VideoFrame& output)
{
    AVFilterBufferRef *bufref;
    int err;

    err = av_buffersrc_write_frame(vbufferCtx_, input.get());
    assert(err >= 0);

    err = av_buffersink_get_buffer_ref(vbufferSinkCtx_, &bufref, 0);
    if (err < 0) {
        WARN("filter get buffersink failed (%d)", err);
        return;
    }
    if (bufref) {
        AVFilterBuffer *avbuf = bufref->buf;
        AVFrame *oframe = output.get();
        av_image_copy(oframe->data, oframe->linesize,
                      (const uint8_t **)avbuf->data, (const int *)avbuf->linesize,
                      (AVPixelFormat)avbuf->format, avbuf->w, avbuf->h);
        avfilter_unref_buffer(bufref);
    }
}

} // end namespace sfl_video
