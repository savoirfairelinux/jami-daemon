/*
 *  Copyright (C) 2011-2013 Savoir-Faire Linux Inc.
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

#ifndef __VIDEO_SCALER_H__
#define __VIDEO_SCALER_H__

#include "video_base.h"
#include "noncopyable.h"


class SwsContext;

namespace sfl_video {

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

private:
    NON_COPYABLE(VideoScaler);
    SwsContext *ctx_;
    int mode_;
    uint8_t *tmp_data_[4]; // used by scale_and_pad
};

}

#endif // __VIDEO_SCALER_H__
