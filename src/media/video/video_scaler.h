/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
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
 */

#ifndef __VIDEO_SCALER_H__
#define __VIDEO_SCALER_H__

#include "video_base.h"
#include "noncopyable.h"


struct SwsContext;

namespace jami { namespace video {

class VideoScaler {
public:
    VideoScaler();
    ~VideoScaler();
    void reset();
    void scale(const VideoFrame &input, VideoFrame &output);
    void scale_with_aspect(const VideoFrame &input, VideoFrame &output);
    void scale_and_pad(const VideoFrame &input, VideoFrame &output,
                       unsigned xoff, unsigned yoff,
                       unsigned dest_width, unsigned dest_height,
                       bool keep_aspect);
    /**
     * @brief convertFormat
     * Converts the frame format to the specified pix format while
     * keeping the frame size and metadata
     * @param input
     * @param pix
     * @return
     */
    std::unique_ptr<VideoFrame> convertFormat(const VideoFrame& input, AVPixelFormat pix);

private:
    NON_COPYABLE(VideoScaler);
    SwsContext *ctx_;
    int mode_;
    uint8_t *tmp_data_[4]; // used by scale_and_pad
};

}} // namespace jami::video

#endif // __VIDEO_SCALER_H__
