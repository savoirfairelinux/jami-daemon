/*
 *  Copyright (C) 2011 Savoir-Faire Linux Inc.
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef __VIDEO_V4L2_H__
#define __VIDEO_V4L2_H__

#include <string>
#include <vector>

extern "C" {
#include <linux/videodev2.h>
#if !defined(VIDIOC_ENUM_FRAMESIZES) || !defined(VIDIOC_ENUM_FRAMEINTERVALS)
#   error You need at least Linux 2.6.19
#endif
}


namespace sfl_video {

class VideoV4l2Rate {
    public:
        VideoV4l2Rate(unsigned num, unsigned den) : den(den), num(num) {}

        unsigned den;
        unsigned num;
};

class VideoV4l2Size {
    public:
        VideoV4l2Size(unsigned height, unsigned width) : height(height), width(width) {}
        void addSupportedRate(const VideoV4l2Rate &rate) {
            rates.push_back(rate);
        }

        unsigned height;
        unsigned width;
        std::vector<VideoV4l2Rate> rates;
};
class VideoV4l2Input {
    public:
        VideoV4l2Input(unsigned idx, const char *s) : idx(idx), name(s) { }

        void addSupportedSize(const VideoV4l2Size &size) {
            sizes.push_back(size);
        }

        std::vector<VideoV4l2Size> sizes;

        void SetFourcc(unsigned code) {
            fourcc[0] = code;
            fourcc[1] = code >> 8;
            fourcc[2] = code >> 16;
            fourcc[3] = code >> 24;
            fourcc[4] = '\0';
        }

        const char * const GetFourcc() { return fourcc; }
        unsigned idx;
        std::string name;

    private:
        char fourcc[5];
};

class VideoV4l2Device {
    public:
        VideoV4l2Device(int fd, std::string &device);

    std::vector<VideoV4l2Input> inputs;
    void addInput(const VideoV4l2Input &input) {
        inputs.push_back(input);
    }

    std::string device;
};

} // namespace sfl_video

#endif //__VIDEO_V4L2_H__ 
