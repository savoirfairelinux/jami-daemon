/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
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

namespace jami { namespace video {

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
        NSArray* observers;
};


VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor) :
    monitor_(monitor)
{
}

void VideoDeviceMonitorImpl::start()
{
    /* Enumerate existing devices */
    auto myVideoDevices = [[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]
                           arrayByAddingObjectsFromArray:
                           [AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]];
    int deviceCount = [myVideoDevices count];
    if (deviceCount > 0)
    {
        for (int ivideo = 0; ivideo < deviceCount; ++ivideo)
        {
            AVCaptureDevice* avf_device = [myVideoDevices objectAtIndex:ivideo];
            printf("avcapture %d/%d %s %s\n", ivideo + 1,
                   deviceCount,
                   [[avf_device modelID] UTF8String],
                   [[avf_device uniqueID] UTF8String]);
            try {
                monitor_->addDevice([[avf_device uniqueID] UTF8String]);
            } catch (const std::runtime_error &e) {
                JAMI_ERR("%s", e.what());
            }
        }
    }

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    id deviceWasConnectedObserver = [notificationCenter addObserverForName:AVCaptureDeviceWasConnectedNotification
                                                                    object:nil
                                                                     queue:[NSOperationQueue mainQueue]
                                                                usingBlock:^(NSNotification *note) {
        auto dev = (AVCaptureDevice*)note.object;
        if([dev hasMediaType:AVMediaTypeVideo])
            monitor_->addDevice([[dev uniqueID] UTF8String]);
    }];
    id deviceWasDisconnectedObserver = [notificationCenter addObserverForName:AVCaptureDeviceWasDisconnectedNotification
                                                                       object:nil
                                                                        queue:[NSOperationQueue mainQueue]
                                                                   usingBlock:^(NSNotification *note) {
        auto dev = (AVCaptureDevice*)note.object;
        if([dev hasMediaType:AVMediaTypeVideo])
            monitor_->removeDevice([[dev uniqueID] UTF8String]);
    }];
    observers = [[NSArray alloc] initWithObjects:deviceWasConnectedObserver, deviceWasDisconnectedObserver, nil];
}

VideoDeviceMonitorImpl::~VideoDeviceMonitorImpl()
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    for (id observer in observers)
        [notificationCenter removeObserver:observer];

    [observers release];
}

VideoDeviceMonitor::VideoDeviceMonitor() :
    preferences_(), devices_(),
    monitorImpl_(new VideoDeviceMonitorImpl(this))
{
    monitorImpl_->start();
    addDevice(DEVICE_DESKTOP, {});
}

VideoDeviceMonitor::~VideoDeviceMonitor()
{}

}} // namespace jami::video
