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
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../video_device_monitor.h"
#include "logger.h"
#include "noncopyable.h"
#include "string_utils.h"

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace jami {
namespace video {

// Constants
namespace {
    constexpr const char* VIDEO4LINUX_SUBSYSTEM = "video4linux";
    constexpr const char* DEV_PREFIX = "/dev";
    constexpr const char* VIDEO_DEVICE_PREFIX = "/dev/video";
    constexpr int SELECT_TIMEOUT_USEC = 500000; // 500ms
}

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

    // Helper methods for initialization
    bool initializeUdev();
    bool setupUdevMonitor();
    void enumerateExistingDevices();
    void addDeviceFromUdev(struct udev_device* dev);
    void fallbackToDirectDeviceEnumeration();
    void cleanup();
    
    // Helper methods for device management
    std::string getUniqueDeviceName(struct udev_device* dev) const;
    bool isValidDevicePath(const char* path) const;
    void handleUdevEvent();
    void processDeviceAction(struct udev_device* dev, const char* action, const char* path);
};

std::string
VideoDeviceMonitorImpl::getUniqueDeviceName(struct udev_device* udev_device) const
{
    if (auto serial = udev_device_get_property_value(udev_device, "ID_SERIAL"))
        return serial;
    throw std::invalid_argument("No ID_SERIAL detected");
}

bool
VideoDeviceMonitorImpl::isValidDevicePath(const char* path) const
{
    return path && starts_with(path, DEV_PREFIX);
}

bool
VideoDeviceMonitorImpl::initializeUdev()
{
    udev_ = udev_new();
    return udev_ != nullptr;
}

bool
VideoDeviceMonitorImpl::setupUdevMonitor()
{
    udev_mon_ = udev_monitor_new_from_netlink(udev_, "udev");
    if (!udev_mon_)
        return false;
    
    return udev_monitor_filter_add_match_subsystem_devtype(udev_mon_, VIDEO4LINUX_SUBSYSTEM, nullptr) == 0;
}

void
VideoDeviceMonitorImpl::addDeviceFromUdev(struct udev_device* dev)
{
    const char* path = udev_device_get_devnode(dev);
    if (!isValidDevicePath(path)) {
        return;
    }
    
    try {
        auto unique_name = getUniqueDeviceName(dev);
        JAMI_LOG("udev: adding device with id {}", unique_name);
        if (monitor_->addDevice(unique_name, {{{"devPath", path}}}))
            currentPathToId_.emplace(path, unique_name);
    } catch (const std::exception& e) {
        JAMI_WARNING("udev: {}, fallback on path (your camera may be a fake camera)", e.what());
        if (monitor_->addDevice(path, {{{"devPath", path}}}))
            currentPathToId_.emplace(path, path);
    }
}

void
VideoDeviceMonitorImpl::enumerateExistingDevices()
{
    udev_enumerate* devenum = udev_enumerate_new(udev_);
    if (!devenum) {
        throw std::runtime_error("Failed to create udev enumerator");
    }
    
    if (udev_enumerate_add_match_subsystem(devenum, VIDEO4LINUX_SUBSYSTEM)) {
        udev_enumerate_unref(devenum);
        throw std::runtime_error("Failed to add subsystem match");
    }

    udev_monitor_enable_receiving(udev_mon_);
    /* Note that we enumerate _after_ monitoring is enabled so that we do not
     * lose device events occurring while we are enumerating. We could still
     * lose events if the Netlink socket receive buffer overflows. */
    udev_enumerate_scan_devices(devenum);
    
    udev_list_entry* devlist = udev_enumerate_get_list_entry(devenum);
    struct udev_list_entry* deventry;
    
    udev_list_entry_foreach(deventry, devlist) {
        const char* path = udev_list_entry_get_name(deventry);
        struct udev_device* dev = udev_device_new_from_syspath(udev_, path);
        if (dev) {
            addDeviceFromUdev(dev);
            udev_device_unref(dev);
        }
    }
    
    udev_enumerate_unref(devenum);
}

void
VideoDeviceMonitorImpl::fallbackToDirectDeviceEnumeration()
{
    JAMI_WARNING("udev enumeration failed, falling back to direct device enumeration");
    
    /* fallback : go through /dev/video* */
    for (int idx = 0;; ++idx) {
        try {
            std::string devicePath = VIDEO_DEVICE_PREFIX + std::to_string(idx);
            if (!monitor_->addDevice(devicePath))
                break;
        } catch (const std::runtime_error& e) {
            JAMI_ERR("%s", e.what());
            return;
        }
    }
}

void
VideoDeviceMonitorImpl::cleanup()
{
    if (udev_mon_) {
        udev_monitor_unref(udev_mon_);
        udev_mon_ = nullptr;
    }
    if (udev_) {
        udev_unref(udev_);
        udev_ = nullptr;
    }
}

VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor)
    : monitor_(monitor)
    , thread_()
    , mutex_()
    , udev_(nullptr)
    , udev_mon_(nullptr)
    , probing_(false)
{
    try {
        if (!initializeUdev()) {
            throw std::runtime_error("Failed to initialize udev");
        }
        
        if (!setupUdevMonitor()) {
            throw std::runtime_error("Failed to setup udev monitor");
        }
        
        enumerateExistingDevices();
        
    } catch (const std::exception& e) {
        JAMI_ERR("udev initialization failed: {}", e.what());
        cleanup();
        fallbackToDirectDeviceEnumeration();
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
    cleanup();
}

void
VideoDeviceMonitorImpl::handleUdevEvent()
{
    udev_device* dev = udev_monitor_receive_device(udev_mon_);
    if (!dev) {
        return;
    }
    
    const char* path = udev_device_get_devnode(dev);
    if (!isValidDevicePath(path)) {
        udev_device_unref(dev);
        return;
    }
    
    const char* action = udev_device_get_action(dev);
    if (action) {
        processDeviceAction(dev, action, path);
    }
    
    udev_device_unref(dev);
}

void
VideoDeviceMonitorImpl::processDeviceAction(struct udev_device* dev, const char* action, const char* path)
{
    try {
        if (!strcmp(action, "add")) {
            auto unique_name = getUniqueDeviceName(dev);
            JAMI_LOG("udev: adding device with id {}", unique_name);
            if (monitor_->addDevice(unique_name, {{{"devPath", path}}})) {
                currentPathToId_.emplace(path, unique_name);
            }
        } else if (!strcmp(action, "remove")) {
            auto it = currentPathToId_.find(path);
            if (it != currentPathToId_.end()) {
                JAMI_LOG("udev: removing {}", it->second);
                monitor_->removeDevice(it->second);
                currentPathToId_.erase(it);
            } else {
                // In case of fallback
                JAMI_LOG("udev: removing {}", path);
                monitor_->removeDevice(path);
            }
        }
    } catch (const std::exception& e) {
        JAMI_ERROR("Error processing device action '{}' for {}: {}", action, path, e.what());
    }
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
        timeval timeout = {0 /* sec */, SELECT_TIMEOUT_USEC /* usec */};
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(udev_fd, &read_set);

        int select_result = select(udev_fd + 1, &read_set, nullptr, nullptr, &timeout);
        
        switch (select_result) {
        case 0:
            // Timeout - continue monitoring
            break;
            
        case 1:
            handleUdevEvent();
            break;
            
        case -1:
            if (errno == EAGAIN)
                continue;
            JAMI_ERROR("udev monitoring thread: select failed ({})", strerror(errno));
            probing_ = false;
            return;
            
        default:
            JAMI_ERROR("select() returned unexpected value {:d} ({})", select_result, strerror(errno));
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
