/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
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

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "logger.h"
#include "video_device.h"

namespace sfl_video {

using std::map;
using std::string;
using std::vector;

template <class T>
static inline string
to_string(const T& t)
{
    std::stringstream ss;
    ss << t;
    return ss.str();
}

static inline int
to_int(const string& str)
{
    int i;
    std::istringstream(str) >> i;
    return i;
}

void
VideoSettings::fromMap(const map<string, string>& map)
{
    for (const auto& pair : map) {
        string key = pair.first;
        string value = pair.second;

        if (key == "name") {
            name = value;
        } else if (key == "channel") {
            channel = value;
        } else if (key == "size") {
            const size_t x = value.find('x');
            if (x != string::npos) {
                height = to_int(value.substr(x + 1));
                width = to_int(value.erase(x));
            }
        } else if (key == "rate") {
            rate = to_int(value);
        } else {
            DEBUG("ignoring key %s", key.data());
        }
    }
}

map<string, string>
VideoSettings::toMap() const
{
    map<string, string> map;

    map["name"] = name;
    map["input"] = node;
    map["mrl"] = "v4l2://" + node;

    map["channel_num"] = to_string(channel_num);
    map["channel"] = to_string(channel);

    map["width"] = to_string(width);
    map["height"] = to_string(height);
    std::stringstream size;
    size << width;
    size << "x";
    size << height;
    map["video_size"] = size.str();
    map["size"] = size.str();

    map["framerate"] = to_string(rate);
    map["rate"] = to_string(rate);

    return map;
}

void
VideoSettings::print() const
{
    WARN("VideoSettings: name:%s node:%s channel_num:%d channel:%s width:%d height: %d rate:%d",
            name.data(), node.data(), channel_num, channel.data(), width, height, rate);
}

} // namespace sfl_video
