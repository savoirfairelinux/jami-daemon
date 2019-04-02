/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
 *
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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
 */

#include <mutex>
#include <thread>

#include "../video_device_monitor.h"
#include "logger.h"
#include "noncopyable.h"

namespace jami { namespace video {

class VideoDeviceMonitorImpl
{
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
        bool probing_;
        std::thread thread_;
};

VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor)
    : monitor_(monitor)
    , mutex_()
    , thread_()
{}

void
VideoDeviceMonitorImpl::start()
{
    probing_ = true;
    thread_ = std::thread(&VideoDeviceMonitorImpl::run, this);
}

VideoDeviceMonitorImpl::~VideoDeviceMonitorImpl()
{
    probing_ = false;
    if (thread_.joinable())
        thread_.join();
}

void
VideoDeviceMonitorImpl::run()
{
}

VideoDeviceMonitor::VideoDeviceMonitor()
    : preferences_()
    , devices_()
    , monitorImpl_(new VideoDeviceMonitorImpl(this))
{
    monitorImpl_->start();
}

VideoDeviceMonitor::~VideoDeviceMonitor()
{}

}} // namespace jami::video
