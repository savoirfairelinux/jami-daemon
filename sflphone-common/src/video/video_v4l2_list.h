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

#ifndef __VIDEO_V4L2_LIST_H__
#define __VIDEO_V4L2_LIST_H__

#include <string>
#include <vector>
#include <cc++/thread.h>
#include <libudev.h>

#include "video_v4l2.h"


namespace sfl_video {

class VideoV4l2List : public ost::Thread {
    public:
        VideoV4l2List();
        ~VideoV4l2List();

        virtual void run();
        virtual void finalize();

        std::vector<std::string> getDeviceList(void);
        std::vector<std::string> getChannelList(const std::string &dev);
        std::vector<std::string> getSizeList(const std::string &dev, const std::string &channel);
        std::vector<std::string> getRateList(const std::string &dev, const std::string &channel, const std::string &size);

        VideoV4l2Device &getDevice(const std::string &name);
        const std::string &getDeviceNode(const std::string &name);
        unsigned getChannelNum(const std::string &dev, const std::string &name);

    private:
        /**
         * @throw std::runtime_error
         */
        void delDevice(const std::string &node);
        bool addDevice(const std::string &dev);
        std::vector<VideoV4l2Device> devices;
        ost::Mutex _mutex;

        struct udev *_udev;
        struct udev_monitor *_udev_mon;
};

} // namespace sfl_video

#endif //__VIDEO_V4L2_LIST_H__ 
