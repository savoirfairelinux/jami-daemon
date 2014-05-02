/*
 *  Copyright (C) 2011-2013 Savoir-Faire Linux Inc.
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

#ifndef __VIDEO_V4L2_LIST_H__
#define __VIDEO_V4L2_LIST_H__

#include <string>
#include <vector>
#include <libudev.h>

#include "video_v4l2.h"
#include <mutex>
#include <thread>
#include "noncopyable.h"

namespace sfl_video {

class VideoV4l2ListThread {
    public:
        VideoV4l2ListThread();
        ~VideoV4l2ListThread();
        void start();

        std::vector<std::string> getDeviceList();
        std::vector<std::string> getChannelList(const std::string &dev);
        std::vector<std::string> getSizeList(const std::string &dev, const std::string &channel);
        std::vector<std::string> getRateList(const std::string &dev, const std::string &channel, const std::string &size);

        std::string getDeviceNode(const std::string &name);
        unsigned getChannelNum(const std::string &dev, const std::string &name);

    private:
        void run();

        std::vector<VideoV4l2Device>::const_iterator findDevice(const std::string &name) const;
        NON_COPYABLE(VideoV4l2ListThread);
        void delDevice(const std::string &node);
        bool addDevice(const std::string &dev);
        std::vector<VideoV4l2Device> devices_;
        std::thread thread_;
        std::mutex mutex_;

        udev *udev_;
        udev_monitor *udev_mon_;
        bool probing_;
};

} // namespace sfl_video

#endif // __VIDEO_V4L2_LIST_H__
