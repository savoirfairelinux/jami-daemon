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
#pragma once

#include "noncopyable.h"
#include "observer.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wshadow"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <set>
#include <mutex>
#include <ciso646> // fix windows compiler bug

extern "C" {
#include <libavutil/pixfmt.h>

struct AVPacket;
struct AVDictionary;

#ifndef AVFORMAT_AVIO_H
struct AVIOContext;
#endif
}

namespace libjami {
class MediaFrame;
class VideoFrame;
} // namespace libjami

namespace jami {
using MediaFrame = libjami::MediaFrame;
using VideoFrame = libjami::VideoFrame;
} // namespace jami

namespace jami {
namespace video {

struct VideoFrameActiveWriter : Observable<std::shared_ptr<MediaFrame>>
{};
struct VideoFramePassiveReader : Observer<std::shared_ptr<MediaFrame>>
{};

/*=== VideoGenerator =========================================================*/

class VideoGenerator : public VideoFrameActiveWriter
{
public:
    VideoGenerator() {}

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual AVPixelFormat getPixelFormat() const = 0;

    std::shared_ptr<VideoFrame> obtainLastFrame();

public:
    // getNewFrame and publishFrame must be called by the same thread only
    VideoFrame& getNewFrame();
    void publishFrame();
    void publishFrame(std::shared_ptr<VideoFrame>);
    void flushFrames();

private:
    std::shared_ptr<VideoFrame> writableFrame_ = nullptr;
    std::shared_ptr<VideoFrame> lastFrame_ = nullptr;
    std::mutex mutex_ {}; // lock writableFrame_/lastFrame_ access
};

struct VideoSettings
{
    VideoSettings() {}
    VideoSettings(const std::map<std::string, std::string>& settings);

    std::map<std::string, std::string> to_map() const;

    std::string unique_id {};
    std::string input {};
    std::string name {};
    std::string channel {};
    std::string video_size {};
    std::string framerate {};
};

} // namespace video
} // namespace jami

namespace YAML {
template<>
struct convert<jami::video::VideoSettings>
{
    static Node encode(const jami::video::VideoSettings& rhs);
    static bool decode(const Node& node, jami::video::VideoSettings& rhs);
};

Emitter& operator<<(Emitter& out, const jami::video::VideoSettings& v);

} // namespace YAML
