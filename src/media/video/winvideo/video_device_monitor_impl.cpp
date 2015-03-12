/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
 *  Author: Edric Milaret <edric.ladent-milaret@savoirfairelinux.com>
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
#include <cerrno>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <stdexcept> // for std::runtime_error
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../video_device_monitor.h"
#include "logger.h"
#include "noncopyable.h"

#include <vfw.h>

namespace ring { namespace video {

class VideoDeviceMonitorImpl {
    public:
        /*
         * This is the only restriction to the pImpl design:
         * as the Linux implementation has a thread, it needs a way to notify
         * devices addition and deletion.
         *
         * This class should maybe inherit from VideoDeviceMonitor instead of
         * being its pImpl.
         */
        VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor);
        ~VideoDeviceMonitorImpl();

        void start();

    private:
        NON_COPYABLE(VideoDeviceMonitorImpl);

        VideoDeviceMonitor* monitor_;

        void run();

        mutable std::mutex mutex_;
        //std::atomic_bool probing_;
        std::thread thread_;
};

VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor) :
    monitor_(monitor),
    mutex_(),
    thread_()
{
    /* Enumerate existing devices */
    char szDeviceName[80];
    char szDeviceVersion[80];
    for (WORD wIndex = 0; wIndex < 10; wIndex++)
    {
        RING_DBG("SEARCHING CAM");
        if (capGetDriverDescription(
            wIndex,
            szDeviceName,
            sizeof (szDeviceName),
            szDeviceVersion,
            sizeof (szDeviceVersion)
            ))
        {
            try {
                RING_DBG("FOUND CAM");
                monitor_->addDevice(std::to_string(wIndex));
            } catch (const std::runtime_error &e) {
                RING_ERR("%s", e.what());
            }
        }
    }

    //TODO Alert when no device found

    return;
}

void VideoDeviceMonitorImpl::start()
{
    //probing_ = true;
    thread_ = std::thread(&VideoDeviceMonitorImpl::run, this);
}

VideoDeviceMonitorImpl::~VideoDeviceMonitorImpl()
{
    //probing_ = false;
    if (thread_.joinable())
        thread_.join();
}

void VideoDeviceMonitorImpl::run()
{
    while (true) {
        //TODO: Enable detection of new devices
        sleep(1);
    }
}

VideoDeviceMonitor::VideoDeviceMonitor() :
    preferences_(), devices_(),
    monitorImpl_(new VideoDeviceMonitorImpl(this))
{
    monitorImpl_->start();
}

VideoDeviceMonitor::~VideoDeviceMonitor()
{}

}} //ring::video namespace