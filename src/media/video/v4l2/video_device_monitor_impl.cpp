/*
 *  Copyright (C) 2009 Rémi Denis-Courmont
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <libudev.h>
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

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace jami {
namespace video {

using std::vector;
using std::string;

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

    std::map<std::string, std::string> currentPathToId_ {};

private:
    NON_COPYABLE(VideoDeviceMonitorImpl);

    VideoDeviceMonitor* monitor_;

    void run();
    std::thread thread_;
    mutable std::mutex mutex_;

    udev* udev_;
    udev_monitor* udev_mon_;
    bool probing_;
};

std::string
getDeviceString(struct udev_device* udev_device)
{
    if (auto serial = udev_device_get_property_value(udev_device, "ID_SERIAL"))
        return serial;
    throw std::invalid_argument("No ID_SERIAL detected");
}

static int
is_v4l2(struct udev_device* dev)
{
    const char* version = udev_device_get_property_value(dev, "ID_V4L_VERSION");
    /* we do not support video4linux 1 */
    return version and strcmp(version, "1");
}

VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor)
    : monitor_(monitor)
    , thread_()
    , mutex_()
    , udev_(0)
    , udev_mon_(0)
    , probing_(false)
{
    udev_list_entry* devlist;
    udev_enumerate* devenum;

    udev_ = udev_new();
    if (!udev_)
        goto udev_failed;

    udev_mon_ = udev_monitor_new_from_netlink(udev_, "udev");
    if (!udev_mon_)
        goto udev_failed;
    if (udev_monitor_filter_add_match_subsystem_devtype(udev_mon_, "video4linux", NULL))
        goto udev_failed;

    /* Enumerate existing devices */
    devenum = udev_enumerate_new(udev_);
    if (devenum == NULL)
        goto udev_failed;
    if (udev_enumerate_add_match_subsystem(devenum, "video4linux")) {
        udev_enumerate_unref(devenum);
        goto udev_failed;
    }

    udev_monitor_enable_receiving(udev_mon_);
    /* Note that we enumerate _after_ monitoring is enabled so that we do not
     * loose device events occuring while we are enumerating. We could still
     * loose events if the Netlink socket receive buffer overflows. */
    udev_enumerate_scan_devices(devenum);
    devlist = udev_enumerate_get_list_entry(devenum);
    struct udev_list_entry* deventry;
    udev_list_entry_foreach(deventry, devlist)
    {
        const char* path = udev_list_entry_get_name(deventry);
        struct udev_device* dev = udev_device_new_from_syspath(udev_, path);

        if (is_v4l2(dev)) {
            const char* path = udev_device_get_devnode(dev);
            if (path && std::string(path).find("/dev") != 0) {
                // udev_device_get_devnode will fail
                continue;
            }
            try {
                auto unique_name = getDeviceString(dev);
                JAMI_DBG("udev: adding device with id %s", unique_name.c_str());
                if (monitor_->addDevice(unique_name, {{{"devPath", path}}}))
                    currentPathToId_.emplace(path, unique_name);
            } catch (const std::exception& e) {
                JAMI_WARN("udev: %s, fallback on path (your camera may be a fake camera)", e.what());
                if (monitor_->addDevice(path, {{{"devPath", path}}}))
                    currentPathToId_.emplace(path, path);
            }
        }
        udev_device_unref(dev);
    }
    udev_enumerate_unref(devenum);

    return;

udev_failed:

    JAMI_ERR("udev enumeration failed");

    if (udev_mon_)
        udev_monitor_unref(udev_mon_);
    if (udev_)
        udev_unref(udev_);
    udev_mon_ = NULL;
    udev_ = NULL;

    /* fallback : go through /dev/video* */
    for (int idx = 0;; ++idx) {
        try {
            if (!monitor_->addDevice("/dev/video" + std::to_string(idx)))
                break;
        } catch (const std::runtime_error& e) {
            JAMI_ERR("%s", e.what());
            return;
        }
    }
}

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
    if (udev_mon_)
        udev_monitor_unref(udev_mon_);
    if (udev_)
        udev_unref(udev_);
}

void
VideoDeviceMonitorImpl::run()
{
    if (!udev_mon_) {
        probing_ = false;
        return;
    }

    const int udev_fd = udev_monitor_get_fd(udev_mon_);
    while (probing_) {
        timeval timeout = {0 /* sec */, 500000 /* usec */};
        fd_set set;
        FD_ZERO(&set);
        FD_SET(udev_fd, &set);

        int ret = select(udev_fd + 1, &set, NULL, NULL, &timeout);
        switch (ret) {
        case 0:
            break;
        case 1: {
            udev_device* dev = udev_monitor_receive_device(udev_mon_);
            if (is_v4l2(dev)) {
                const char* path = udev_device_get_devnode(dev);
                if (path && std::string(path).find("/dev") != 0) {
                    // udev_device_get_devnode will fail
                    break;
                }
                try {
                    auto unique_name = getDeviceString(dev);

                    const char* action = udev_device_get_action(dev);
                    if (!strcmp(action, "add")) {
                        JAMI_DBG("udev: adding device with id %s", unique_name.c_str());
                        if (monitor_->addDevice(unique_name, {{{"devPath", path}}}))
                            currentPathToId_.emplace(path, unique_name);
                    } else if (!strcmp(action, "remove")) {
                        auto it = currentPathToId_.find(path);
                        if (it != currentPathToId_.end()) {
                            JAMI_DBG("udev: removing %s", it->second.c_str());
                            monitor_->removeDevice(it->second);
                            currentPathToId_.erase(it);
                        } else {
                            // In case of fallback
                            JAMI_DBG("udev: removing %s", path);
                            monitor_->removeDevice(path);
                        }
                    }
                } catch (const std::exception& e) {
                    JAMI_ERR("%s", e.what());
                }
            }
            udev_device_unref(dev);
            break;
        }

        case -1:
            if (errno == EAGAIN)
                continue;
            JAMI_ERR("udev monitoring thread: select failed (%m)");
            probing_ = false;
            return;

        default:
            JAMI_ERR("select() returned %d (%m)", ret);
            probing_ = false;
            return;
        }
    }
}

VideoDeviceMonitor::VideoDeviceMonitor()
    : preferences_()
    , devices_()
    , monitorImpl_(new VideoDeviceMonitorImpl(this))
{
    monitorImpl_->start();
    addDevice(DEVICE_DESKTOP, {});
}

VideoDeviceMonitor::~VideoDeviceMonitor() {}

} // namespace video
} // namespace jami
