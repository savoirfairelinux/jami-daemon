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

#include <vector>
#include <climits>

#include <iostream>
using namespace std;

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
}

#include "video_v4l2.h"

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

namespace sfl_video
{

static int GetFrameRates(int fd, VideoV4l2Size &size, unsigned int pixel_format)
{
    struct v4l2_frmivalenum frmival = {
        0,
        pixel_format,
        size.width,
        size.height,
    };

    if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival)) {
        return 1; // could not query frame interval for size
    }

    switch(frmival.type) {
    case V4L2_FRMIVAL_TYPE_DISCRETE:
        do {
            VideoV4l2Rate rate(frmival.discrete.numerator,
                           frmival.discrete.denominator);
            size.addSupportedRate(rate);
            frmival.index++;
        } while (!ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival));
        break;
    case V4L2_FRMIVAL_TYPE_CONTINUOUS:
        // TODO
        cerr << "Continuous Frame Intervals not supported" << endl;
        return 2;
    case V4L2_FRMIVAL_TYPE_STEPWISE:
        // TODO
        cerr << "Stepwise Frame Intervals not supported" << endl;
        return 3;
    }

    return 0;
}

static int GetSizes(int fd, VideoV4l2Input &input, unsigned int pixelformat)
{
    struct v4l2_frmsizeenum frmsize;
    frmsize.index = 0;
    frmsize.pixel_format = pixelformat;
    if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize))
        return 1; // could not query frame sizes for format

    switch(frmsize.type) {
    case V4L2_FRMSIZE_TYPE_DISCRETE:
        do {
            VideoV4l2Size size(frmsize.discrete.height, frmsize.discrete.width);

            if (!GetFrameRates(fd, size, frmsize.pixel_format))
                input.addSupportedSize(size);

            frmsize.index++;
        } while (!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize));
        break;

        // TODO, we dont want to display a list of 2000x2000 
        // resolutions if the camera supports continuous framesizes
        // from 1x1 to 2000x2000
        // We should limit to a list of known standard sizes
    case V4L2_FRMSIZE_TYPE_CONTINUOUS:
        cerr << "Continuous Frame sizes not supported" << endl;
        return 2;
    case V4L2_FRMSIZE_TYPE_STEPWISE:
        cerr << "Stepwise Frame sizes not supported" << endl;
        return 3;
    }

    return 0;
}

static int GetFormat(int fd, VideoV4l2Input &input)
{
    int idx = input.idx;
    if (ioctl(fd, VIDIOC_S_INPUT, &idx))
        return 1;

    struct v4l2_fmtdesc fmt;
    fmt.index = idx = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    unsigned int best_score = UINT_MAX;
    unsigned int best_idx = 0;
    unsigned int pixelformat;
    while (!ioctl(fd, VIDIOC_ENUM_FMT, &fmt)) {
        if (idx != fmt.index)
            break;
       
        unsigned int score = pixelformat_score(fmt.pixelformat);
        if (score < best_score) {
            pixelformat = fmt.pixelformat;
            best_idx = idx;
            best_score = score;
        }

        fmt.index = ++idx;
    }
    if (idx == 0)
        return 2;

    fmt.index = best_idx;
    input.SetFourcc(pixelformat);

    int ret = GetSizes(fd, input, pixelformat);
    return ret ? ret + 2 : ret;
}

VideoV4l2Device::VideoV4l2Device(const char *dev) {
    int idx;
    int fd = open(dev, O_RDWR);
    if (fd == -1)
        throw 1; // could not open device

    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        close(fd);
        throw 2; // could not query capabilities
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        close(fd);
        throw 3; // not a capture device
    }

    struct v4l2_input input;
    input.index = idx = 0;
    while (!ioctl(fd, VIDIOC_ENUMINPUT, &input)) {
        if (idx != input.index)
            break;

        if (input.type & V4L2_INPUT_TYPE_CAMERA) {
            VideoV4l2Input vinput(idx, (const char*)input.name);

            if (GetFormat(fd, vinput)) {
                close(fd);
                throw 4; // could not add input
            }

            addInput(vinput);
        }

        input.index = ++idx;
    }

    close(fd);
}

} // end namespace sfl
