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
#include <sstream>

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
        VideoV4l2Size(unsigned height, unsigned width);

        /**
         * @throw std::runtime_error
         */
        void GetFrameRates(int fd, unsigned int pixel_format);
        std::vector<std::string> getRateList();


        void setRate(unsigned index);
        unsigned getRateIndex();
        VideoV4l2Rate &getRate();

        unsigned height;
        unsigned width;

    private:
        std::vector<VideoV4l2Rate> rates;
        unsigned _currentRate;
};

class VideoV4l2Channel {
    public:
        VideoV4l2Channel(unsigned idx, const char *s);

        /**
         * @throw std::runtime_error
         */
        void GetFormat(int fd);
        /**
         * @throw std::runtime_error
         */
        unsigned int GetSizes(int fd, unsigned int pixel_format);

        void SetFourcc(unsigned code);
        const char * GetFourcc();

        void setSize(unsigned index);

        std::vector<std::string> getSizeList(void);
        unsigned getSizeIndex();

        VideoV4l2Size &getSize();

        unsigned idx;
        std::string name;

    private:
        std::vector<VideoV4l2Size> sizes;
        char fourcc[5];
        unsigned _currentSize;
};

class VideoV4l2Device {
    public:
        /**
         * @throw std::runtime_error
         */
        VideoV4l2Device(int fd, const std::string &device);

        std::string device;
        std::string name;

        std::vector<std::string> getChannelList(void);
        void setChannel(unsigned index);
        unsigned getChannelIndex();
        VideoV4l2Channel &getChannel();

    private:
        std::vector<VideoV4l2Channel> channels;
        unsigned _currentChannel;
};

} // namespace sfl_video

#endif //__VIDEO_V4L2_H__ 
