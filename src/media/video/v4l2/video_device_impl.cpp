/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
 *
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <linux/videodev2.h>
#if !defined(VIDIOC_ENUM_FRAMESIZES) || !defined(VIDIOC_ENUM_FRAMEINTERVALS)
#   error You need at least Linux 2.6.19
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
}

#include "logger.h"
#include "../video_device.h"
#include "string_utils.h"
#include "array_size.h"

#define ZEROVAR(x) std::memset(&(x), 0, sizeof(x))

namespace jami { namespace video {

class VideoV4l2Rate
{
public:
    VideoV4l2Rate(unsigned rate_numerator = 0,
                  unsigned rate_denominator = 0,
                  unsigned format = 0)
        : frame_rate(rate_denominator , rate_numerator), pixel_format(format) {}

    FrameRate frame_rate;
    unsigned pixel_format;
    std::string libAvPixelformat() const;
};

class VideoV4l2Size
{
public:
    VideoV4l2Size(const unsigned width, const unsigned height)
        : width(width), height(height), rates_() {}

    /**
     * @throw std::runtime_error
     */
    void readFrameRates(int fd, unsigned int pixel_format);

    std::vector<FrameRate> getRateList() const;
    VideoV4l2Rate getRate(const FrameRate& rate) const;

    unsigned width;
    unsigned height;

private:
    void addRate(VideoV4l2Rate proposed_rate);
    std::vector<VideoV4l2Rate> rates_;
};

bool
operator==(VideoV4l2Size& a, VideoV4l2Size& b)
{
    return a.height == b.height && a.width == b.width;
}

class VideoV4l2Channel
{
public:
    VideoV4l2Channel(unsigned idx, const char *s);

    /**
     * @throw std::runtime_error
     */
    void readFormats(int fd);

    /**
     * @throw std::runtime_error
     */
    unsigned int readSizes(int fd, unsigned int pixel_format);

    std::vector<VideoSize> getSizeList() const;
    const VideoV4l2Size& getSize(VideoSize name) const;

    unsigned idx;
    std::string name;

private:
    void putCIFFirst();
    std::vector<VideoV4l2Size> sizes_;
};

class VideoDeviceImpl
{
public:
    /**
     * @throw std::runtime_error
     */
    VideoDeviceImpl(const std::string& path);

    std::string id;
    std::string name;

    std::vector<std::string> getChannelList() const;
    std::vector<VideoSize> getSizeList(const std::string& channel) const;
    std::vector<FrameRate> getRateList(const std::string& channel, VideoSize size) const;

    DeviceParams getDeviceParams() const;
    void setDeviceParams(const DeviceParams&);

private:
    std::vector<VideoV4l2Channel> channels_;
    const VideoV4l2Channel& getChannel(const std::string& name) const;

    /* Preferences */
    VideoV4l2Channel channel_;
    VideoV4l2Size size_;
    VideoV4l2Rate rate_;
};

static const unsigned pixelformats_supported[] = {
    /* pixel format        depth  description   */

    /* preferred formats, they can be fed directly to the video encoder */
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

    /* Compressed formats */
    V4L2_PIX_FMT_MJPEG,
    V4L2_PIX_FMT_JPEG,
    V4L2_PIX_FMT_DV,
    V4L2_PIX_FMT_MPEG,
    V4L2_PIX_FMT_H264,
    V4L2_PIX_FMT_H264_NO_SC,
    V4L2_PIX_FMT_H264_MVC,
    V4L2_PIX_FMT_H263,
    V4L2_PIX_FMT_MPEG1,
    V4L2_PIX_FMT_MPEG2,
    V4L2_PIX_FMT_MPEG4,
    V4L2_PIX_FMT_XVID,
    V4L2_PIX_FMT_VC1_ANNEX_G,
    V4L2_PIX_FMT_VC1_ANNEX_L,
    V4L2_PIX_FMT_VP8,

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
 * See pixelformats_supported array for the support list.
 */
static unsigned int
pixelformat_score(unsigned pixelformat)
{
    unsigned int formats_count = arraySize(pixelformats_supported);
    for (unsigned int i = 0; i < formats_count; ++i) {
        if (pixelformats_supported[i] == pixelformat)
            return i;
    }
    return UINT_MAX - 1;
}

using std::vector;
using std::string;
using std::stringstream;

vector<FrameRate>
VideoV4l2Size::getRateList() const
{
    vector<FrameRate> rates;
    rates.reserve(rates_.size());
    for (const auto& r : rates_)
        rates.emplace_back(r.frame_rate);
    return rates;
}

void
VideoV4l2Size::readFrameRates(int fd, unsigned int pixel_format)
{
    VideoV4l2Rate fallback_rate {1, 25, pixel_format};

    v4l2_frmivalenum frmival;
    ZEROVAR(frmival);
    frmival.pixel_format = pixel_format;
    frmival.width = width;
    frmival.height = height;

    if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival)) {
        addRate(fallback_rate);
        JAMI_ERR("could not query frame interval for size");
        return;
    }

    if (frmival.type != V4L2_FRMIVAL_TYPE_DISCRETE) {
        addRate(fallback_rate);
        JAMI_ERR("Continuous and stepwise Frame Intervals are not supported");
        return;
    }

    do {
        addRate({frmival.discrete.numerator, frmival.discrete.denominator, pixel_format});
        ++frmival.index;
    } while (!ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival));
}

VideoV4l2Rate
VideoV4l2Size::getRate(const FrameRate& rate) const
{
    for (const auto& item : rates_) {
        if (std::fabs((item.frame_rate - rate).real()) < 0.0001)
            return item;
    }
    return rates_.back();
}

void
VideoV4l2Size::addRate(VideoV4l2Rate new_rate)
{
    bool rate_found = false;
    for (auto& item : rates_) {
        if (item.frame_rate == new_rate.frame_rate) {
            if (pixelformat_score(item.pixel_format) > pixelformat_score(new_rate.pixel_format)) {
                 // Make sure we will use the prefered pixelformat (lower score means prefered format)
                item.pixel_format = new_rate.pixel_format;
            }
            rate_found = true;
        }
    }

    if (!rate_found)
        rates_.push_back(new_rate);
}

VideoV4l2Channel::VideoV4l2Channel(unsigned idx, const char *s)
    : idx(idx)
    , name(s)
    , sizes_()
{}

std::vector<VideoSize>
VideoV4l2Channel::getSizeList() const
{
    vector<VideoSize> v;
    v.reserve(sizes_.size());
    for (const auto &item : sizes_)
        v.emplace_back(item.width, item.height);

    return v;
}

unsigned int
VideoV4l2Channel::readSizes(int fd, unsigned int pixelformat)
{
    v4l2_frmsizeenum frmsize;
    ZEROVAR(frmsize);

    frmsize.index = 0;
    frmsize.pixel_format = pixelformat;

    if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) < 0) {
        v4l2_format fmt;
        ZEROVAR(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
            throw std::runtime_error("Could not get format");

        VideoV4l2Size size(fmt.fmt.pix.width, fmt.fmt.pix.height);
        size.readFrameRates(fd, fmt.fmt.pix.pixelformat);
        sizes_.push_back(size);

        return fmt.fmt.pix.pixelformat;
    }

    if (frmsize.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
        // We do not take care of V4L2_FRMSIZE_TYPE_CONTINUOUS or V4L2_FRMSIZE_TYPE_STEPWISE
        JAMI_ERR("Continuous Frame sizes not supported");
        return pixelformat;
    }

    // Real work starts here: attach framerates to sizes and update pixelformat information
    do {
        bool size_exists = false;
        VideoV4l2Size size(frmsize.discrete.width, frmsize.discrete.height);

        for (auto &item : sizes_) {
            if (item == size) {
                size_exists = true;
                // If a size already exist we add frame rates since there may be some
                // frame rates available in one format that are not availabe in another.
                item.readFrameRates(fd, frmsize.pixel_format);
            }
        }

        if (!size_exists) {
            size.readFrameRates(fd, frmsize.pixel_format);
            sizes_.push_back(size);
        }

        ++frmsize.index;
    } while (!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize));

    return pixelformat;
}

// Put CIF resolution (352x288) first in the list since it is more prevalent in
// VoIP
void
VideoV4l2Channel::putCIFFirst()
{
    const auto iter = std::find_if(sizes_.begin(), sizes_.end(),
                                   [] (const VideoV4l2Size& size) {
                                       return size.width == 352 and size.height == 258;
                                   });

    if (iter != sizes_.end() and iter != sizes_.begin())
        std::swap(*iter, *sizes_.begin());
}

void
VideoV4l2Channel::readFormats(int fd)
{
    if (ioctl(fd, VIDIOC_S_INPUT, &idx))
        throw std::runtime_error("VIDIOC_S_INPUT failed");

    v4l2_fmtdesc fmt;
    ZEROVAR(fmt);
    unsigned fmt_index = 0;

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (!ioctl(fd, VIDIOC_ENUM_FMT, &fmt)) {
        if (fmt_index != fmt.index)
            break;
        readSizes(fd, fmt.pixelformat);
        fmt.index = ++fmt_index;
    }

    if (fmt_index == 0)
        throw std::runtime_error("Could not enumerate formats");

    putCIFFirst();
}

const VideoV4l2Size&
VideoV4l2Channel::getSize(VideoSize s) const
{
    for (const auto &item : sizes_) {
        if (item.width == s.first && item.height == s.second)
            return item;
    }

    assert(not sizes_.empty());
    return sizes_.front();
}

VideoDeviceImpl::VideoDeviceImpl(const string& path)
    : id(path)
    , name()
    , channels_()
    , channel_(-1, "")
    , size_(-1, -1)
    , rate_(-1, 1, 0)
{
    int fd = open(id.c_str(), O_RDWR);
    if (fd == -1)
        throw std::runtime_error("could not open device");

    v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap))
        throw std::runtime_error("could not query capabilities");

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        throw std::runtime_error("not a capture device");

    name = string(reinterpret_cast<const char*>(cap.card));

    v4l2_input input;
    ZEROVAR(input);
    unsigned idx;
    input.index = idx = 0;
    while (!ioctl(fd, VIDIOC_ENUMINPUT, &input)) {
        if (idx != input.index)
            break;

        if (input.type & V4L2_INPUT_TYPE_CAMERA) {
            VideoV4l2Channel channel(idx, (const char*) input.name);
            channel.readFormats(fd);
            if (not channel.getSizeList().empty())
                channels_.push_back(channel);
        }

        input.index = ++idx;
    }

    ::close(fd);
}

string
VideoV4l2Rate::libAvPixelformat() const
{
    switch (pixel_format) {
        // Set codec name for those pixelformats.
        // Those  names can be found in libavcodec/codec_desc.c
        case V4L2_PIX_FMT_MJPEG:
            return "mjpeg";
        case V4L2_PIX_FMT_DV:
            return "dvvideo";
        case V4L2_PIX_FMT_MPEG:
        case V4L2_PIX_FMT_MPEG1:
            return "mpeg1video";
        case V4L2_PIX_FMT_H264:
        case V4L2_PIX_FMT_H264_NO_SC:
        case V4L2_PIX_FMT_H264_MVC:
            return "h264";
        case V4L2_PIX_FMT_H263:
            return "h263";
        case V4L2_PIX_FMT_MPEG2:
            return "mpeg2video";
        case V4L2_PIX_FMT_MPEG4:
            return "mpeg4";
        case V4L2_PIX_FMT_VC1_ANNEX_G:
        case V4L2_PIX_FMT_VC1_ANNEX_L:
            return "vc1";
        case V4L2_PIX_FMT_VP8:
            return "vp8";
        default: // Most pixel formats do not need any codec
            return "";
    }
}

vector<string>
VideoDeviceImpl::getChannelList() const
{
    vector<string> v;
    v.reserve(channels_.size());
    for (const auto &itr : channels_)
        v.push_back(itr.name);

    return v;
}

vector<VideoSize>
VideoDeviceImpl::getSizeList(const string& channel) const
{
    return getChannel(channel).getSizeList();
}

vector<FrameRate>
VideoDeviceImpl::getRateList(const string& channel, VideoSize size) const
{
    return getChannel(channel).getSize(size).getRateList();
}

const VideoV4l2Channel&
VideoDeviceImpl::getChannel(const string &name) const
{
    for (const auto &item : channels_)
        if (item.name == name)
            return item;

    assert(not channels_.empty());
    return channels_.front();
}

DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;
    params.name = name;
    params.input = id;
    params.format = "video4linux2";
    params.channel_name = channel_.name;
    params.channel = channel_.idx;
    params.width = size_.width;
    params.height = size_.height;
    params.framerate = rate_.frame_rate;
    params.pixel_format = rate_.libAvPixelformat();
    return params;
}

void
VideoDeviceImpl::setDeviceParams(const DeviceParams& params)
{
    // Set preferences or fallback to defaults.
    channel_ = getChannel(params.channel_name);
    size_ = channel_.getSize({params.width, params.height});
    try {
        rate_ = size_.getRate(params.framerate);
    } catch (...) {
        rate_ = {};
    }
}

VideoDevice::VideoDevice(const std::string& path, const std::vector<std::map<std::string, std::string>>&)
    : deviceImpl_(new VideoDeviceImpl(path))
{
    id_ = path;
    name = deviceImpl_->name;
}

DeviceParams
VideoDevice::getDeviceParams() const
{
    auto params = deviceImpl_->getDeviceParams();
    params.orientation = orientation_;
    return params;
}

void
VideoDevice::setDeviceParams(const DeviceParams& params)
{
    return deviceImpl_->setDeviceParams(params);
}

std::vector<std::string>
VideoDevice::getChannelList() const
{
    return deviceImpl_->getChannelList();
}

std::vector<VideoSize>
VideoDevice::getSizeList(const std::string& channel) const
{
    return deviceImpl_->getSizeList(channel);
}

std::vector<FrameRate>
VideoDevice::getRateList(const std::string& channel, VideoSize size) const
{
    return deviceImpl_->getRateList(channel, size);
}

VideoDevice::~VideoDevice()
{}

}} // namespace jami::video
