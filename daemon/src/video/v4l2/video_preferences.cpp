/*
 *  Copyright (C) 2009 Rémi Denis-Courmont
 *
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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
#include <cstring>
#include <libudev.h>
#include <mutex>
#include <sstream>
#include <stdexcept> // for std::runtime_error
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../video_preferences.h"
#include "client/videomanager.h"
#include "config/yamlemitter.h"
#include "config/yamlnode.h"
#include "logger.h"
#include "manager.h"
#include "noncopyable.h"
#include "video_v4l2.h"

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace sfl_video {

using std::vector;
using std::string;

class VideoDeviceMonitorImpl {
    public:
        VideoDeviceMonitorImpl();
        ~VideoDeviceMonitorImpl();
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
        NON_COPYABLE(VideoDeviceMonitorImpl);
        void delDevice(const std::string &node);
        bool addDevice(const std::string &dev);
        std::vector<VideoV4l2Device> devices_;
        std::thread thread_;
        std::mutex mutex_;

        udev *udev_;
        udev_monitor *udev_mon_;
        bool probing_;
};

static int is_v4l2(struct udev_device *dev)
{
    const char *version = udev_device_get_property_value(dev, "ID_V4L_VERSION");
    /* we do not support video4linux 1 */
    return version and strcmp(version, "1");
}

VideoDeviceMonitorImpl::VideoDeviceMonitorImpl() : devices_(),
    thread_(), mutex_(), udev_(0),
    udev_mon_(0), probing_(false)
{
    udev_list_entry *devlist;
    udev_enumerate *devenum;

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
    struct udev_list_entry *deventry;
    udev_list_entry_foreach(deventry, devlist) {
        const char *path = udev_list_entry_get_name(deventry);
        struct udev_device *dev = udev_device_new_from_syspath(udev_, path);

        if (is_v4l2(dev)) {
            const char *devpath = udev_device_get_devnode(dev);
            if (devpath) {
                try {
                    addDevice(devpath);
                } catch (const std::runtime_error &e) {
                    ERROR("%s", e.what());
                }
            }
        }
        udev_device_unref(dev);
    }
    udev_enumerate_unref(devenum);

    return;

udev_failed:

    ERROR("udev enumeration failed");

    if (udev_mon_)
        udev_monitor_unref(udev_mon_);
    if (udev_)
        udev_unref(udev_);
    udev_mon_ = NULL;
    udev_ = NULL;

    /* fallback : go through /dev/video* */
    for (int idx = 0;; ++idx) {
        std::stringstream ss;
        ss << "/dev/video" << idx;
        try {
            if (!addDevice(ss.str()))
                return;
        } catch (const std::runtime_error &e) {
            ERROR("%s", e.what());
            return;
        }
    }
}

void VideoDeviceMonitorImpl::start()
{
    probing_ = true;
    thread_ = std::thread(&VideoDeviceMonitorImpl::run, this);
}

namespace {

    typedef std::vector<VideoV4l2Device> Devices;
    struct DeviceComparator {
        explicit DeviceComparator(const std::string &name) : name_(name) {}
        inline bool operator()(const VideoV4l2Device &d) const { return d.name == name_; }
        private:
        const std::string name_;
    };

    int getNumber(const string &name, size_t *sharp)
    {
        size_t len = name.length();
        // name is too short to be numbered
        if (len < 3)
            return -1;

        for (size_t c = len; c; --c) {
            if (name[c] == '#') {
                unsigned i;
                if (sscanf(name.substr(c).c_str(), "#%u", &i) != 1)
                    return -1;
                *sharp = c;
                return i;
            }
        }

        return -1;
    }

    void giveUniqueName(VideoV4l2Device &dev, const vector<VideoV4l2Device> &devices)
    {
start:
        for (auto &item : devices) {
            if (dev.name == item.name) {
                size_t sharp;
                int num = getNumber(dev.name, &sharp);
                if (num < 0) // not numbered
                    dev.name += " #0";
                else {
                    std::stringstream ss;
                    ss  << num + 1;
                    dev.name.replace(sharp + 1, ss.str().length(), ss.str());
                }
                goto start; // we changed the name, let's look again if it is unique
            }
        }
    }
} // end anonymous namespace

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

void VideoDeviceMonitorImpl::run()
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
            case 1:
                {
                    udev_device *dev = udev_monitor_receive_device(udev_mon_);
                    if (!is_v4l2(dev)) {
                        udev_device_unref(dev);
                        break;
                    }

                    const char *node = udev_device_get_devnode(dev);
                    const char *action = udev_device_get_action(dev);
                    if (!strcmp(action, "add")) {
                        DEBUG("udev: adding %s", node);
                        try {
                            if (addDevice(node)) {
                                Manager::instance().getVideoManager()->deviceEvent();
                            }
                        } catch (const std::runtime_error &e) {
                            ERROR("%s", e.what());
                        }
                    } else if (!strcmp(action, "remove")) {
                        DEBUG("udev: removing %s", node);
                        delDevice(string(node));
                    }
                    udev_device_unref(dev);
                    break;
                }

            case -1:
                if (errno == EAGAIN)
                    continue;
                ERROR("udev monitoring thread: select failed (%m)");
                probing_ = false;
                return;

            default:
                ERROR("select() returned %d (%m)", ret);
                probing_ = false;
                return;
        }
    }
}

void VideoDeviceMonitorImpl::delDevice(const string &node)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto itr = std::find_if(devices_.begin(), devices_.end(),
            [&] (const VideoV4l2Device &d) { return d.device == node; });

    if (itr != devices_.end()) {
        devices_.erase(itr);
        Manager::instance().getVideoManager()->deviceEvent();
    }
}

bool VideoDeviceMonitorImpl::addDevice(const string &dev)
{
    std::lock_guard<std::mutex> lock(mutex_);

    int fd = open(dev.c_str(), O_RDWR);
    if (fd == -1) {
        ERROR("Problem opening device");
        strErr();
        return false;
    }

    const string s(dev);
    VideoV4l2Device v(fd, s);
    giveUniqueName(v, devices_);
    devices_.push_back(v);

    ::close(fd);
    return true;
}

vector<string>
VideoDeviceMonitorImpl::getChannelList(const string &dev)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Devices::const_iterator iter(findDevice(dev));
    if (iter != devices_.end())
        return iter->getChannelList();
    else
        return vector<string>();
}

vector<string>
VideoDeviceMonitorImpl::getSizeList(const string &dev, const string &channel)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Devices::const_iterator iter(findDevice(dev));
    if (iter != devices_.end())
        return iter->getChannel(channel).getSizeList();
    else
        return vector<string>();
}

vector<string>
VideoDeviceMonitorImpl::getRateList(const string &dev, const string &channel, const std::string &size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Devices::const_iterator iter(findDevice(dev));
    if (iter != devices_.end())
        return iter->getChannel(channel).getSize(size).getRateList();
    else
        return vector<string>();
}

vector<string> VideoDeviceMonitorImpl::getDeviceList()
{
    std::lock_guard<std::mutex> lock(mutex_);
    vector<string> v;

    for (const auto &itr : devices_)
       v.push_back(itr.name.empty() ? itr.device : itr.name);

    return v;
}

Devices::const_iterator
VideoDeviceMonitorImpl::findDevice(const string &name) const
{
    Devices::const_iterator iter(std::find_if(devices_.begin(), devices_.end(), DeviceComparator(name)));
    if (iter == devices_.end())
        ERROR("Device %s not found", name.c_str());
    return iter;
}

unsigned VideoDeviceMonitorImpl::getChannelNum(const string &dev, const string &name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Devices::const_iterator iter(findDevice(dev));
    if (iter != devices_.end())
        return iter->getChannel(name).idx;
    else
        return 0;
}

string VideoDeviceMonitorImpl::getDeviceNode(const string &name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Devices::const_iterator iter(findDevice(name));
    if (iter != devices_.end())
        return iter->device;
    else
        return "";
}
} // namespace sfl_video

using namespace sfl_video;

VideoPreference::VideoPreference() :
    monitorImpl_(new VideoDeviceMonitorImpl),
    deviceList_()
{
    monitorImpl_->start();
}

VideoPreference::~VideoPreference()
{}

/*
 * V4L2 interface.
 */

std::vector<std::string>
VideoPreference::getDeviceList()
{
    return monitorImpl_->getDeviceList();
}

std::vector<std::string>
VideoPreference::getChannelList(const std::string &dev)
{
    return monitorImpl_->getChannelList(dev);
}

std::vector<std::string>
VideoPreference::getSizeList(const std::string &dev, const std::string &channel)
{
    return monitorImpl_->getSizeList(dev, channel);
}

std::vector<std::string>
VideoPreference::getRateList(const std::string &dev, const std::string &channel, const std::string &size)
{
    return monitorImpl_->getRateList(dev, channel, size);
}

/*
 * Interface for a single device.
 */

std::map<std::string, std::string>
VideoPreference::deviceToSettings(const VideoDevice& dev)
{
    std::map<std::string, std::string> settings;

    settings["input"] = monitorImpl_->getDeviceNode(dev.name);

    std::stringstream channel_index;
    channel_index << monitorImpl_->getChannelNum(dev.name, dev.channel);
    settings["channel"] = channel_index.str();

    settings["video_size"] = dev.size;
    size_t x_pos = dev.size.find('x');
    settings["width"] = dev.size.substr(0, x_pos);
    settings["height"] = dev.size.substr(x_pos + 1);

    settings["framerate"] = dev.rate;

    return settings;
}
