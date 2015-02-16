/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
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
#include "video_base.h"
#include "string_utils.h"
#include "logger.h"

namespace ring { namespace video {

/*=== VideoPacket  ===========================================================*/

VideoPacket::VideoPacket() : packet_(static_cast<AVPacket *>(av_mallocz(sizeof(AVPacket))))
{
    av_init_packet(packet_);
}

VideoPacket::~VideoPacket() { av_free_packet(packet_); av_free(packet_); }

/*=== VideoFrame =============================================================*/

VideoFrame::VideoFrame()
{
    frame_ = av_frame_alloc(); // FIXME: error handling
}

VideoFrame::~VideoFrame()
{
#if USE_OLD_AVU
    if (allocated_)
        avpicture_free((AVPicture *) frame_);
#endif
    av_frame_free(&frame_);
}

int VideoFrame::getPixelFormat() const
{ return libav_utils::sfl_pixel_format(frame_->format); }

int VideoFrame::getWidth() const
{ return frame_->width; }

int VideoFrame::getHeight() const
{ return frame_->height; }

bool VideoFrame::allocBuffer(int width, int height, int pix_fmt)
{
    AVPixelFormat libav_pix_fmt = (AVPixelFormat) libav_utils::libav_pixel_format(pix_fmt);
    if (allocated_) {
        // nothing to do if same properties
        if (width == frame_->width
            and height == frame_->height
            and libav_pix_fmt == frame_->format)
            return true;
#if USE_OLD_AVU
        avpicture_free((AVPicture *) frame_);
#else
        av_frame_unref(frame_);
#endif
    }

    setGeometry(width, height, pix_fmt);
    allocated_ = not av_frame_get_buffer(frame_, 32);
    return allocated_;
}

void VideoFrame::setdefaults()
{
    av_frame_unref(frame_);
#if !USE_OLD_AVU
    allocated_ = false;
#endif
}

void VideoFrame::setGeometry(int width, int height, int pix_fmt)
{
    frame_->format = libav_utils::libav_pixel_format(pix_fmt);
    frame_->width = width;
    frame_->height = height;
}

void VideoFrame::setDestination(void *data, int width, int height, int pix_fmt)
{
    setdefaults();
    setGeometry(width, height, pix_fmt);
    avpicture_fill((AVPicture *)frame_, (uint8_t *)data,
                   (AVPixelFormat)frame_->format, frame_->width,
                   frame_->height);
}

size_t VideoFrame::getSize()
{
    return avpicture_get_size((AVPixelFormat) frame_->format,
                              frame_->width,
                              frame_->height);
}

size_t VideoFrame::getSize(int width, int height, int format)
{
    return avpicture_get_size(
        (AVPixelFormat) libav_utils::libav_pixel_format(format), width, height);
}

void VideoFrame::copy(VideoFrame &dst)
{
    const AVFrame *dst_frame = dst.get();
    dst.allocBuffer(frame_->width, frame_->height, getPixelFormat());
    av_picture_copy((AVPicture *)dst_frame, (AVPicture *)frame_,
                    (AVPixelFormat)frame_->format, frame_->width,
                    frame_->height);
}

void VideoFrame::clone(VideoFrame &dst)
{
#if USE_OLD_AVU
    copy(dst);
#else
    dst.setdefaults();
    av_frame_ref(dst.frame_, frame_);
#endif
}

void VideoFrame::clear()
{
    // FIXME: beurk!!!!

    memset(frame_->data[0], 0, frame_->linesize[0]*frame_->height);
    // 128 is the black level for U/V channels
    memset(frame_->data[1], 128, frame_->linesize[1]*frame_->height/2);
    memset(frame_->data[2], 128, frame_->linesize[2]*frame_->height/2);
}

/*=== VideoGenerator =========================================================*/

VideoFrame& VideoGenerator::getNewFrame()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (writableFrame_)
        writableFrame_->setdefaults();
    else
        writableFrame_.reset(new VideoFrame());
    return *writableFrame_.get();
}

void VideoGenerator::publishFrame()
{
    std::lock_guard<std::mutex> lk(mutex_);
    lastFrame_ = std::move(writableFrame_);
    notify(lastFrame_);
}

void VideoGenerator::flushFrames()
{
    std::lock_guard<std::mutex> lk(mutex_);
    writableFrame_.reset();
    lastFrame_.reset();
}

std::shared_ptr<VideoFrame> VideoGenerator::obtainLastFrame()
{
    std::lock_guard<std::mutex> lk(mutex_);
    return lastFrame_;
}

/*=== VideoSettings =========================================================*/

static std::string
extractString(const std::map<std::string, std::string>& settings, const std::string& key) {
    auto i = settings.find(key);
    if (i != settings.cend())
        return i->second;
    return {};
}

static unsigned
extractInt(const std::map<std::string, std::string>& settings, const std::string& key) {
    auto i = settings.find(key);
    if (i != settings.cend())
        return std::stoi(i->second);
    return 0;
}

VideoSettings::VideoSettings(const std::map<std::string, std::string>& settings)
{
    name = extractString(settings, "name");
    channel = extractString(settings, "channel");
    video_size = extractString(settings, "size");
    framerate = extractInt(settings, "rate");
}

std::map<std::string, std::string>
VideoSettings::to_map() const
{
    std::map<std::string, std::string> settings;
    settings["name"] = name;
    settings["size"] = video_size;
    settings["channel"] = channel;
    settings["rate"] = ring::to_string(framerate);
    return settings;
}

}} // namespace ring::video

namespace YAML {

Node
convert<ring::video::VideoSettings>::encode(const ring::video::VideoSettings& rhs) {
    Node node;
    node["name"] = rhs.name;
    node["video_size"] = rhs.video_size;
    node["channel"] = rhs.channel;
    node["framerate"] = ring::to_string(rhs.framerate);
    return node;
}

bool
convert<ring::video::VideoSettings>::decode(const Node& node, ring::video::VideoSettings& rhs) {
    if (not node.IsMap()) {
        RING_WARN("Can't decode VideoSettings YAML node");
        return false;
    }
    rhs.name = node["name"].as<std::string>();
    rhs.video_size = node["video_size"].as<std::string>();
    rhs.channel = node["channel"].as<std::string>();
    rhs.framerate = node["framerate"].as<unsigned>();
    return true;
}

Emitter& operator << (Emitter& out, const ring::video::VideoSettings& v) {
    out << convert<ring::video::VideoSettings>::encode(v);
    return out;
}

} // namespace YAML
