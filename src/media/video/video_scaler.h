/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#ifndef __VIDEO_SCALER_H__
#define __VIDEO_SCALER_H__

#include "video_base.h"
#include "noncopyable.h"

struct SwsContext;

extern "C" {
struct AVFrame;
}

namespace jami {
namespace video {

class VideoScaler
{
public:
    VideoScaler();
    ~VideoScaler();
    void reset();
    void scale(const AVFrame* input, AVFrame* output);
    void scale(const VideoFrame& input, VideoFrame& output);
    void scale_with_aspect(const VideoFrame& input, VideoFrame& output);
    void scale_and_pad(const VideoFrame& input,
                       VideoFrame& output,
                       unsigned xoff,
                       unsigned yoff,
                       unsigned dest_width,
                       unsigned dest_height,
                       bool keep_aspect);
    std::unique_ptr<VideoFrame> convertFormat(const VideoFrame& input, AVPixelFormat pix);

private:
    NON_COPYABLE(VideoScaler);
    SwsContext* ctx_;
    int mode_;
    uint8_t* tmp_data_[4]; // used by scale_and_pad
};

} // namespace video
} // namespace jami

#endif // __VIDEO_SCALER_H__
