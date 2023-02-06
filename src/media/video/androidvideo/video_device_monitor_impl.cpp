/*
 *  Copyright (C) 2009 Rémi Denis-Courmont
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 */

#include "../video_device_monitor.h"
#include "logger.h"
#include "noncopyable.h"

#include "client/ring_signal.h"

#include <algorithm>
#include <mutex>
#include <sstream>
#include <stdexcept> // for std::runtime_error
#include <string>
#include <thread>
#include <vector>

namespace jami {
namespace video {

using std::vector;
using std::string;

class VideoDeviceMonitorImpl
{
    /*
     * This class is instantiated in VideoDeviceMonitor's constructor. The
     * daemon has a global VideoManager, and it contains a VideoDeviceMonitor.
     * So, when the library is loaded on Android, VideoDeviceMonitorImpl will
     * be instantiated before we get a chance to register callbacks. At this
     * point, if we use emitSignal to get a list of cameras, it will simply
     * do nothing.
     * To work around this issue, functions have been added in the video
     * manager interface to allow to add/remove devices from Java code.
     * To conclude, this class is just an empty stub.
     */
public:
    VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor);
    ~VideoDeviceMonitorImpl();

    void start();

private:
    NON_COPYABLE(VideoDeviceMonitorImpl);
};

VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor) {}

void
VideoDeviceMonitorImpl::start()
{}

VideoDeviceMonitorImpl::~VideoDeviceMonitorImpl() {}

VideoDeviceMonitor::VideoDeviceMonitor()
    : preferences_()
    , devices_()
    , monitorImpl_(new VideoDeviceMonitorImpl(this))
{}

VideoDeviceMonitor::~VideoDeviceMonitor() {}

} // namespace video
} // namespace jami
