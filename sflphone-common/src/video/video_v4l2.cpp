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

#include <string>
#include <vector>
#include <climits>
#include <stdexcept>

#include "logger.h"

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
}

#include "video_v4l2.h"

namespace sfl_video
{
static const unsigned pixelformats_supported[] = {
    /* pixel format        depth  description   */

    /* prefered formats, they can be fed directly to the video encoder */
    V4L2_PIX_FMT_YUV420,   /* 12  YUV 4:2:0     */
    V4L2_PIX_FMT_YUV422P,  /* 16  YVU422 planar */
    V4L2_PIX_FMT_YUV444,   /* 16  xxxxyyyy uuuuvvvv */

    /* Luminance+Chrominance formats */
    V4L2_PIX_FMT_YVU410,   /*  9  YVU 4:1:0     */
    V4L2_PIX_FMT_YVU420,   /* 12  YVU 4:2:0     */
    V4L2_PIX_FMT_YUYV,     /* 16  YUV 4:2:2     */
    V4L2_PIX_FMT_YYUV,     /* 16  YUV 4:2:2     */
    V4L2_PIX_FMT_YVYU,     /* 16 YVU 4:2:2 */
    V4L2_PIX_FMT_UYVY,     /* 16  YUV 4:2:2     */
    V4L2_PIX_FMT_VYUY,     /* 16  YUV 4:2:2     */
    V4L2_PIX_FMT_YUV411P,  /* 16  YVU411 planar */
    V4L2_PIX_FMT_Y41P,     /* 12  YUV 4:1:1     */
    V4L2_PIX_FMT_YUV555,   /* 16  YUV-5-5-5     */
    V4L2_PIX_FMT_YUV565,   /* 16  YUV-5-6-5     */
    V4L2_PIX_FMT_YUV32,    /* 32  YUV-8-8-8-8   */
    V4L2_PIX_FMT_YUV410,   /*  9  YUV 4:1:0     */
    V4L2_PIX_FMT_HI240,    /*  8  8-bit color   */
    V4L2_PIX_FMT_HM12,     /*  8  YUV 4:2:0 16x16 macroblocks */

    /* two planes -- one Y, one Cr + Cb interleaved  */
    V4L2_PIX_FMT_NV12,     /* 12  Y/CbCr 4:2:0  */
    V4L2_PIX_FMT_NV21,     /* 12  Y/CrCb 4:2:0  */
    V4L2_PIX_FMT_NV16,     /* 16  Y/CbCr 4:2:2  */
    V4L2_PIX_FMT_NV61,     /* 16  Y/CrCb 4:2:2  */

#if 0
    /* RGB formats */
    V4L2_PIX_FMT_RGB332,   /*  8  RGB-3-3-2     */
    V4L2_PIX_FMT_RGB444,   /* 16  xxxxrrrr ggggbbbb */
    V4L2_PIX_FMT_RGB555,   /* 16  RGB-5-5-5     */
    V4L2_PIX_FMT_RGB565,   /* 16  RGB-5-6-5     */
    V4L2_PIX_FMT_RGB555X,  /* 16  RGB-5-5-5 BE  */
    V4L2_PIX_FMT_RGB565X,  /* 16  RGB-5-6-5 BE  */
    V4L2_PIX_FMT_BGR666,   /* 18  BGR-6-6-6     */
    V4L2_PIX_FMT_BGR24,    /* 24  BGR-8-8-8     */
    V4L2_PIX_FMT_RGB24,    /* 24  RGB-8-8-8     */
    V4L2_PIX_FMT_BGR32,    /* 32  BGR-8-8-8-8   */
    V4L2_PIX_FMT_RGB32,    /* 32  RGB-8-8-8-8   */

    /* Grey formats */
    V4L2_PIX_FMT_GREY,     /*  8  Greyscale     */
    V4L2_PIX_FMT_Y4,       /*  4  Greyscale     */
    V4L2_PIX_FMT_Y6,       /*  6  Greyscale     */
    V4L2_PIX_FMT_Y10,      /* 10  Greyscale     */
    V4L2_PIX_FMT_Y16,      /* 16  Greyscale     */

    /* Palette formats */
    V4L2_PIX_FMT_PAL8,     /*  8  8-bit palette */
#endif
};

/* Returns a score for the given pixelformat
 *
 * Lowest score is the best, the first entries in the array are the formats
 * supported as an input for the video encoders.
 *
 * Other entries in the array are YUV formats
 *
 * RGB / grey / palette formats are not supported because most cameras support
 * YUV input
 *
 */

static unsigned int pixelformat_score(unsigned pixelformat)
{
    size_t n = sizeof pixelformats_supported / sizeof *pixelformats_supported;
    for (unsigned int i = 0; i < n ; i++) {
        if (pixelformats_supported[i] == pixelformat)
            return i;
    }
    return UINT_MAX - 1;
}

VideoV4l2Size::VideoV4l2Size(unsigned height, unsigned width) : height(height), width(width) {}

std::vector<std::string> VideoV4l2Size::getRateList()
{
	std::vector<std::string> v;
	std::stringstream ss;

	size_t n = rates.size();
	unsigned i;
	for (i = 0 ; i < n ; i++) {
		std::stringstream ss;
		ss << rates[i];
		v.push_back(ss.str());
	}

	return v;
}


void VideoV4l2Size::GetFrameRates(int fd, unsigned int pixel_format)
{
    if (fd == -1) { // SFL_TEST
        rates.push_back(25);
        return;
    }

    struct v4l2_frmivalenum frmival = {
        0,
        pixel_format,
        width,
        height,
    };

    if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival)) {
        rates.push_back(25);
        _error("could not query frame interval for size");
        return;
    }

    switch(frmival.type) {
    case V4L2_FRMIVAL_TYPE_DISCRETE:
        do {
            rates.push_back(frmival.discrete.denominator/frmival.discrete.numerator);
            frmival.index++;
        } while (!ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival));
        break;
    case V4L2_FRMIVAL_TYPE_CONTINUOUS:
        rates.push_back(25);
        // TODO
        _error("Continuous Frame Intervals not supported");
        break;
    case V4L2_FRMIVAL_TYPE_STEPWISE:
        rates.push_back(25);
        // TODO
        _error("Stepwise Frame Intervals not supported");
        break;
    }
}

VideoV4l2Channel::VideoV4l2Channel(unsigned idx, const char *s) : idx(idx), name(s) { }

void VideoV4l2Channel::SetFourcc(unsigned code)
{
    fourcc[0] = code;
    fourcc[1] = code >> 8;
    fourcc[2] = code >> 16;
    fourcc[3] = code >> 24;
    fourcc[4] = '\0';
}

const char * VideoV4l2Channel::GetFourcc()
{
	return fourcc;
}

std::vector<std::string> VideoV4l2Channel::getSizeList(void)
{
    std::vector<std::string> v;

    size_t n = sizes.size();
    unsigned i;
    for (i = 0 ; i < n ; i++) {
        VideoV4l2Size &size = sizes[i];
        std::stringstream ss;
        ss << size.width << "x" << size.height;
        v.push_back(ss.str());
    }

    return v;
}



unsigned int VideoV4l2Channel::GetSizes(int fd, unsigned int pixelformat)
{
    if (fd == -1) { //SFL_TEST
        VideoV4l2Size s(108, 192);
        s.GetFrameRates(-1, 0);
        sizes.push_back(s);
        return 0;
    }

    struct v4l2_frmsizeenum frmsize;
    frmsize.index = 0;
    frmsize.pixel_format = pixelformat;
    if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize))
        goto fallback;

    switch(frmsize.type) {
    case V4L2_FRMSIZE_TYPE_DISCRETE:
        do {
            VideoV4l2Size size(frmsize.discrete.height, frmsize.discrete.width);
            size.GetFrameRates(fd, frmsize.pixel_format);
            sizes.push_back(size);
            frmsize.index++;
        } while (!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize));
        return pixelformat;

        // TODO, we dont want to display a list of 2000x2000 
        // resolutions if the camera supports continuous framesizes
        // from 1x1 to 2000x2000
        // We should limit to a list of known standard sizes
    case V4L2_FRMSIZE_TYPE_CONTINUOUS:
        _error("Continuous Frame sizes not supported");
        break;
    case V4L2_FRMSIZE_TYPE_STEPWISE:
        _error("Stepwise Frame sizes not supported");
        break;
    }

fallback:
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
        throw std::runtime_error("Couldnt get format");
    
    VideoV4l2Size size(fmt.fmt.pix.height, fmt.fmt.pix.width);
    size.GetFrameRates(fd, fmt.fmt.pix.pixelformat);
    sizes.push_back(size);

    return fmt.fmt.pix.pixelformat;
}

void VideoV4l2Channel::GetFormat(int fd)
{
    if (ioctl(fd, VIDIOC_S_INPUT, &idx))
        throw std::runtime_error("VIDIOC_S_INPUT failed");

    struct v4l2_fmtdesc fmt;
    unsigned fmt_index;
    fmt.index = fmt_index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    unsigned int best_score = UINT_MAX;
    unsigned int best_idx = 0;
    unsigned int pixelformat = 0;
    while (!ioctl(fd, VIDIOC_ENUM_FMT, &fmt)) {
        if (fmt_index != fmt.index)
            break;

        unsigned int score = pixelformat_score(fmt.pixelformat);
        if (score < best_score) {
            pixelformat = fmt.pixelformat;
            best_idx = fmt_index;
            best_score = score;
        }

        fmt.index = ++fmt_index;
    }
    if (fmt_index == 0)
        throw std::runtime_error("Could not enumerate formats");

    fmt.index = best_idx;
    pixelformat = GetSizes(fd, pixelformat);

    SetFourcc(pixelformat);
}

VideoV4l2Size VideoV4l2Channel::getSize(const std::string &name)
{
	for (size_t i = 0; i < sizes.size(); i++) {
		std::stringstream ss;
		ss << sizes[i].width << "x" << sizes[i].height;
		if (ss.str() == name)
			return sizes[i];
	}

	throw std::runtime_error("No size found");
}


VideoV4l2Device::VideoV4l2Device(int fd, const std::string &device)
{
    if (fd == -1) {
        VideoV4l2Channel c(0, "#^&");
        c.GetSizes(-1, 0);
        channels.push_back(c);
        name = "TEST";

        this->device = device;
        return;
    }

    unsigned idx;

    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap))
        throw std::runtime_error("could not query capabilities");

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        throw std::runtime_error("not a capture device");

    name = std::string((const char*)cap.card);

    struct v4l2_input input;
    input.index = idx = 0;
    while (!ioctl(fd, VIDIOC_ENUMINPUT, &input)) {
        if (idx != input.index)
            break;

        if (input.type & V4L2_INPUT_TYPE_CAMERA) {
            VideoV4l2Channel channel(idx, (const char*)input.name);
            channel.GetFormat(fd);

            channels.push_back(channel);
        }

        input.index = ++idx;
    }
    this->device = device;
}

std::vector<std::string> VideoV4l2Device::getChannelList(void)
{
    std::vector<std::string> v;

    size_t n = channels.size();
    unsigned i;
    for (i = 0 ; i < n ; i++)
        v.push_back(channels[i].name);

    return v;
}

VideoV4l2Channel &VideoV4l2Device::getChannel(const std::string &name)
{
	for (size_t i = 0; i < channels.size(); i++)
		if (channels[i].name == name)
			return channels[i];

	throw std::runtime_error("No channel found");
}


} // end namespace sfl
