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

#ifndef __VIDEO_DEVICE_H__
#define __VIDEO_DEVICE_H__

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sfl_video {

typedef std::map<std::string, std::map<std::string, std::vector<std::string>>> VideoCapabilities;
typedef std::map<std::string, std::string> VideoSettings;
// FIXME move VideoSettings in video_base since video_decoder (and encoder?) may use it.

class VideoDeviceImpl;

class VideoDevice {
public:

    VideoDevice(const std::string& path);
    ~VideoDevice();

    /*
     * The device name, e.g. "Integrated Camera",
     * actually used as the identifier.
     */
    std::string name = "";

    const std::string& getNode() const { return node_; }

    /*
     * Get the 3 level deep tree of possible settings for the device.
     * The levels are channels, sizes, and rates.
     *
     * The result map for the "Integrated Camera" looks like this:
     *
     *   {'Camera 1': {'1280x720': ['10'],
     *                 '320x240': ['30', '15'],
     *                 '352x288': ['30', '15'],
     *                 '424x240': ['30', '15'],
     *                 '640x360': ['30', '15'],
     *                 '640x480': ['30', '15'],
     *                 '800x448': ['15'],
     *                 '960x540': ['10']}}
     */
    VideoCapabilities getCapabilities() const;

    /*
     * Get the string/string map of settings for the device.
     * The keys are:
     *   - "channel"
     *   - "size"
     *   - "rate"
     *   - TODO ...
     */
    VideoSettings getSettings() const;

    /*
     * Setup the device with the preferences listed in the "settings" map.
     * The expected map should be similar to the result of getSettings().
     *
     * If a key is missing, a valid default value is choosen. Thus, calling
     * this function with an empty map will reset the device to default.
     */
    void applySettings(VideoSettings settings);

private:

    /*
     * The device node, e.g. "/dev/video0".
     */
    std::string node_ = "";

    /*
     * Device specific implementation.
     * On Linux, V4L2 stuffs go there.
     *
     * Note: since a VideoDevice is copyable,
     * deviceImpl_ cannot be an unique_ptr.
     */
    std::shared_ptr<VideoDeviceImpl> deviceImpl_;
};

} // namespace sfl_video

#endif // __VIDEO_DEVICE_H__
