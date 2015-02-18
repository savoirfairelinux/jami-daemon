/*
 *  Copyright (C) 2009 Rémi Denis-Courmont
 *
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#import <AVFoundation/AVFoundation.h>

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
        std::thread thread_;
        mutable std::mutex mutex_;

        bool probing_;
};


VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor) :
    monitor_(monitor),
    thread_(), mutex_(),
    probing_(false)
{
    /* Enumerate existing devices */
    NSArray                 *myVideoDevices;

    myVideoDevices = [[[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo] arrayByAddingObjectsFromArray:[AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]] retain];
    if ( [myVideoDevices count] == 0 )
    {
        RING_ERR("Can't find any suitable video device");
        return;
    }

    int deviceCount = [myVideoDevices count];
    int ivideo;
    for ( ivideo = 0; ivideo < deviceCount; ++ivideo )
    {
        AVCaptureDevice *avf_device;
        avf_device = [myVideoDevices objectAtIndex:ivideo];
        RING_DBG("avcapture %lu/%lu %s %s", ivideo, deviceCount, [[avf_device modelID] UTF8String], [[avf_device uniqueID] UTF8String]);
        try {
            monitor_->addDevice([[avf_device uniqueID] UTF8String]);
        } catch (const std::runtime_error &e) {
            RING_ERR("%s", e.what());
        }
    }

    return;
}

void VideoDeviceMonitorImpl::start()
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

void VideoDeviceMonitorImpl::run()
{
    while (probing_) {
        timeval timeout = {0 /* sec */, 500000 /* usec */};
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

}} // namespace ring::video
