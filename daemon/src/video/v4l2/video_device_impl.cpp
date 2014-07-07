/*
 *  Copyright (C) 2011-2014 Savoir-Faire Linux Inc.
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#define ZEROVAR(x) memset(&(x), 0, sizeof(x))

namespace sfl_video {

class VideoV4l2Size {
    public:
        VideoV4l2Size(unsigned height, unsigned width);

        /**
         * @throw std::runtime_error
         */
        void getFrameRates(int fd, unsigned int pixel_format);
        std::vector<std::string> getRateList() const;
        float getRate(const std::string &name) const;

        unsigned height;
        unsigned width;

    private:
        std::vector<float> rates_;
};

class VideoV4l2Channel {
    public:
        VideoV4l2Channel(unsigned idx, const char *s);

        /**
         * @throw std::runtime_error
         */
        void getFormat(int fd);
        /**
         * @throw std::runtime_error
         */
        unsigned int getSizes(int fd, unsigned int pixel_format);

        void setFourcc(unsigned code);
        const char * getFourcc() const;

        std::vector<std::string> getSizeList() const;
        const VideoV4l2Size& getSize(const std::string &name) const;

        unsigned idx;
        std::string name;

    private:
        void putCIFFirst();
        std::vector<VideoV4l2Size> sizes_;
        char fourcc_[5];
};

class VideoDeviceImpl {
    public:
        /**
         * @throw std::runtime_error
         */
        VideoDeviceImpl(const std::string& path);

        std::string device;
        std::string name;

        std::vector<std::string> getChannelList() const;
        std::vector<std::string> getSizeList(const std::string& channel) const;
        std::vector<std::string> getRateList(const std::string& channel, const std::string& size) const;

        VideoSettings getSettings() const;
        void applySettings(VideoSettings settings);

    private:
        std::vector<VideoV4l2Channel> channels_;
        const VideoV4l2Channel& getChannel(const std::string& name) const;

        /* Preferences */
        VideoV4l2Channel channel_;
        VideoV4l2Size size_;
        float rate_;
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

namespace {
unsigned int pixelformat_score(unsigned pixelformat)
{
    for (const auto &item : pixelformats_supported)
        if (item == pixelformat)
            return item;

    return UINT_MAX - 1;
}
}

using std::vector;
using std::string;
using std::stringstream;

VideoV4l2Size::VideoV4l2Size(unsigned height, unsigned width) :
    height(height), width(width), rates_() {}

vector<string> VideoV4l2Size::getRateList() const
{
    vector<string> v;

    for (const auto &item : rates_) {
        stringstream ss;
        ss << item;
        v.push_back(ss.str());
    }

    return v;
}

void VideoV4l2Size::getFrameRates(int fd, unsigned int pixel_format)
{
    v4l2_frmivalenum frmival;
    ZEROVAR(frmival);
    frmival.pixel_format = pixel_format;
    frmival.width = width;
    frmival.height = height;

    if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival)) {
        rates_.push_back(25);
        ERROR("could not query frame interval for size");
        return;
    }

    switch(frmival.type) {
        case V4L2_FRMIVAL_TYPE_DISCRETE:
            do {
                rates_.push_back(frmival.discrete.denominator/frmival.discrete.numerator);
                ++frmival.index;
            } while (!ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival));
            break;
        case V4L2_FRMIVAL_TYPE_CONTINUOUS:
            rates_.push_back(25);
            // TODO
            ERROR("Continuous Frame Intervals not supported");
            break;
        case V4L2_FRMIVAL_TYPE_STEPWISE:
            rates_.push_back(25);
            // TODO
            ERROR("Stepwise Frame Intervals not supported");
            break;
    }
}

float
VideoV4l2Size::getRate(const string &name) const
{
    for (const auto& item : rates_) {
        stringstream ss;
        ss << item;
        if (ss.str() == name)
            return item;
    }

    // fallback to last size
    assert(not rates_.empty());
    return rates_.back();
}

VideoV4l2Channel::VideoV4l2Channel(unsigned idx, const char *s) :
    idx(idx), name(s), sizes_(), fourcc_() {}

void VideoV4l2Channel::setFourcc(unsigned code)
{
    fourcc_[0] = code;
    fourcc_[1] = code >> 8;
    fourcc_[2] = code >> 16;
    fourcc_[3] = code >> 24;
    fourcc_[4] = '\0';
}

const char *
VideoV4l2Channel::getFourcc() const
{
    return fourcc_;
}

vector<string> VideoV4l2Channel::getSizeList() const
{
    vector<string> v;

    for (const auto &item : sizes_) {
        stringstream ss;
        ss << item.width << "x" << item.height;
        v.push_back(ss.str());
    }

    return v;
}

unsigned int
VideoV4l2Channel::getSizes(int fd, unsigned int pixelformat)
{
    v4l2_frmsizeenum frmsize;
    ZEROVAR(frmsize);
    frmsize.index = 0;
    frmsize.pixel_format = pixelformat;
    if (!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize)) {

        switch (frmsize.type) {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                do {
                    VideoV4l2Size size(frmsize.discrete.height, frmsize.discrete.width);
                    size.getFrameRates(fd, frmsize.pixel_format);
                    sizes_.push_back(size);
                    ++frmsize.index;
                } while (!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize));
                return pixelformat;

                // TODO, we dont want to display a list of 2000x2000
                // resolutions if the camera supports continuous framesizes
                // from 1x1 to 2000x2000
                // We should limit to a list of known standard sizes
            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                ERROR("Continuous Frame sizes not supported");
                break;
            case V4L2_FRMSIZE_TYPE_STEPWISE:
                ERROR("Stepwise Frame sizes not supported");
                break;
        }
    }

    v4l2_format fmt;
    ZEROVAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
        throw std::runtime_error("Could not get format");

    VideoV4l2Size size(fmt.fmt.pix.height, fmt.fmt.pix.width);
    size.getFrameRates(fd, fmt.fmt.pix.pixelformat);
    sizes_.push_back(size);

    return fmt.fmt.pix.pixelformat;
}

namespace {
    bool isCIF(const VideoV4l2Size &size)
    {
        const unsigned CIF_WIDTH = 352;
        const unsigned CIF_HEIGHT = 288;
        return size.width == CIF_WIDTH and size.height == CIF_HEIGHT;
    }
}

// Put CIF resolution (352x288) first in the list since it is more prevalent in
// VoIP
void VideoV4l2Channel::putCIFFirst()
{
    vector<VideoV4l2Size>::iterator iter = std::find_if(sizes_.begin(), sizes_.end(), isCIF);
    if (iter != sizes_.end() and iter != sizes_.begin())
        std::swap(*iter, *sizes_.begin());
}

void VideoV4l2Channel::getFormat(int fd)
{
    if (ioctl(fd, VIDIOC_S_INPUT, &idx))
        throw std::runtime_error("VIDIOC_S_INPUT failed");

    v4l2_fmtdesc fmt;
    ZEROVAR(fmt);
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
    pixelformat = getSizes(fd, pixelformat);
    putCIFFirst();

    setFourcc(pixelformat);
}

const VideoV4l2Size&
VideoV4l2Channel::getSize(const string &name) const
{
    for (const auto &item : sizes_) {
        stringstream ss;
        ss << item.width << "x" << item.height;
        if (ss.str() == name)
            return item;
    }

    // fallback to last size
    assert(not sizes_.empty());
    return sizes_.back();
}

VideoDeviceImpl::VideoDeviceImpl(const string& path) :
    device(path), name(), channels_(),
    channel_(-1, ""), size_(-1, -1), rate_(-1)
{
    int fd = open(device.c_str(), O_RDWR);
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
            channel.getFormat(fd);
            channels_.push_back(channel);
        }

        input.index = ++idx;
    }

    ::close(fd);

    // Set default settings
    applySettings(VideoSettings());
}

vector<string> VideoDeviceImpl::getChannelList() const
{
    vector<string> v;

    for (const auto &itr : channels_)
        v.push_back(itr.name);

    return v;
}

vector<string>
VideoDeviceImpl::getSizeList(const string& channel) const
{
    return getChannel(channel).getSizeList();
}

vector<string>
VideoDeviceImpl::getRateList(const string& channel, const string& size) const
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
    return channels_.back();
}

void
VideoDeviceImpl::applySettings(VideoSettings settings)
{
    // Set preferences or fallback to defaults.
    channel_ = getChannel(settings["channel"]);
    size_ = channel_.getSize(settings["size"]);
    rate_ = size_.getRate(settings["rate"]);
}

template <class T>
static inline string to_string(const T& t)
{
    std::stringstream ss;
    ss << t;
    return ss.str();
}

/*
 * FIXME the result map has duplicated value, for backward compatibility with
 * old methods getPreferences() and getSettings().
 *
 * A VideoSettings struct might be created with eventually a to_map() method.
 */
VideoSettings
VideoDeviceImpl::getSettings() const
{
    VideoSettings settings;

    settings["name"] = name;

    // Device path (e.g. /dev/video0)
    settings["input"] = device;

    // Channel number
    settings["channel_num"] = to_string(channel_.idx);
    settings["channel"] = channel_.name;

    // Video size
    settings["width"] = to_string(size_.width);
    settings["height"] = to_string(size_.height);
    stringstream video_size;
    video_size << settings["width"];
    video_size << "x";
    video_size << settings["height"];
    settings["video_size"] = video_size.str();
    settings["size"] = video_size.str();

    // Frame rate
    settings["framerate"] = to_string(rate_);
    settings["rate"] = to_string(rate_);

    return settings;
}

VideoDevice::VideoDevice(const string& path) :
    deviceImpl_(new VideoDeviceImpl(path))
{
    node_ = path;
    name = deviceImpl_->name;
}

void
VideoDevice::applySettings(VideoSettings settings)
{
    deviceImpl_->applySettings(settings);
}

VideoSettings
VideoDevice::getSettings() const
{
    return deviceImpl_->getSettings();
}

VideoCapabilities
VideoDevice::getCapabilities() const
{
    VideoCapabilities cap;

    for (const auto& chan : deviceImpl_->getChannelList())
        for (const auto& size : deviceImpl_->getSizeList(chan))
            cap[chan][size] = deviceImpl_->getRateList(chan, size);

    return cap;
}

VideoDevice::~VideoDevice()
{}

} // namespace sfl_video
