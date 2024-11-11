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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "video_base.h"
#include "media_buffer.h"
#include "string_utils.h"
#include "logger.h"

#include <cassert>

namespace jami {
namespace video {

/*=== VideoGenerator =========================================================*/

VideoFrame&
VideoGenerator::getNewFrame()
{
    std::lock_guard lk(mutex_);
    writableFrame_.reset(new VideoFrame());
    return *writableFrame_.get();
}

void
VideoGenerator::publishFrame()
{
    std::lock_guard lk(mutex_);
    lastFrame_ = std::move(writableFrame_);
    notify(std::static_pointer_cast<MediaFrame>(lastFrame_));
}

void
VideoGenerator::publishFrame(std::shared_ptr<VideoFrame> frame)
{
    std::lock_guard lk(mutex_);
    lastFrame_ = std::move(frame);
    notify(std::static_pointer_cast<MediaFrame>(lastFrame_));
}

void
VideoGenerator::flushFrames()
{
    std::lock_guard lk(mutex_);
    writableFrame_.reset();
    lastFrame_.reset();
}

std::shared_ptr<VideoFrame>
VideoGenerator::obtainLastFrame()
{
    std::lock_guard lk(mutex_);
    return lastFrame_;
}

/*=== VideoSettings =========================================================*/

static std::string
extractString(const std::map<std::string, std::string>& settings, const std::string& key)
{
    auto i = settings.find(key);
    if (i != settings.cend())
        return i->second;
    return {};
}

VideoSettings::VideoSettings(const std::map<std::string, std::string>& settings)
{
    name = extractString(settings, "name");
    unique_id = extractString(settings, "id");
    input = extractString(settings, "input");
    if (input.empty()) {
        input = unique_id;
    }
    channel = extractString(settings, "channel");
    video_size = extractString(settings, "size");
    framerate = extractString(settings, "rate");
}

std::map<std::string, std::string>
VideoSettings::to_map() const
{
    return {{"name", name},
            {"id", unique_id},
            {"input", input},
            {"size", video_size},
            {"channel", channel},
            {"rate", framerate}};
}

} // namespace video
} // namespace jami

namespace YAML {

Node
convert<jami::video::VideoSettings>::encode(const jami::video::VideoSettings& rhs)
{
    Node node;
    node["name"] = rhs.name;
    node["id"] = rhs.unique_id;
    node["input"] = rhs.input;
    node["video_size"] = rhs.video_size;
    node["channel"] = rhs.channel;
    node["framerate"] = rhs.framerate;
    return node;
}

bool
convert<jami::video::VideoSettings>::decode(const Node& node, jami::video::VideoSettings& rhs)
{
    if (not node.IsMap()) {
        JAMI_WARN("Unable to decode VideoSettings YAML node");
        return false;
    }
    rhs.name = node["name"].as<std::string>();
    rhs.unique_id = node["id"].as<std::string>();
    rhs.input = node["input"].as<std::string>();
    rhs.video_size = node["video_size"].as<std::string>();
    rhs.channel = node["channel"].as<std::string>();
    rhs.framerate = node["framerate"].as<std::string>();
    return true;
}

Emitter&
operator<<(Emitter& out, const jami::video::VideoSettings& v)
{
    out << convert<jami::video::VideoSettings>::encode(v);
    return out;
}

} // namespace YAML
