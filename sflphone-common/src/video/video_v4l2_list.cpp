/*
 *  Copyright (C) 2011 Savoir-Faire Linux Inc.
 *  Copyright © 2009 Rémi Denis-Courmont
 *
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <sstream>

#ifdef HAVE_UDEV
#include <libudev.h>
#include <cstring>
#endif //HAVE_UDEV


extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include "video_v4l2_list.h"

namespace sfl_video {

VideoV4l2List::VideoV4l2List() : _currentDevice(0)
{
#ifdef HAVE_UDEV
    struct udev *udev;
    struct udev_monitor *mon = NULL;
    struct udev_list_entry *devlist;
    struct udev_enumerate *devenum;

    udev = udev_new();
    if (!udev)
        goto udev_error;

    mon = udev_monitor_new_from_netlink (udev, "udev");
    if (!mon)
        goto udev_error;
    if (udev_monitor_filter_add_match_subsystem_devtype (mon, "video4linux", NULL))
        goto udev_error;

    /* Enumerate existing devices */
    devenum = udev_enumerate_new (udev);
    if (devenum == NULL)
        goto udev_error;
    if (udev_enumerate_add_match_subsystem (devenum, "video4linux"))
    {
        udev_enumerate_unref (devenum);
        goto udev_error;
    }

    udev_monitor_enable_receiving (mon);
    /* Note that we enumerate _after_ monitoring is enabled so that we do not
     * loose device events occuring while we are enumerating. We could still
     * loose events if the Netlink socket receive buffer overflows. */
    udev_enumerate_scan_devices (devenum);
    devlist = udev_enumerate_get_list_entry (devenum);
    struct udev_list_entry *deventry;
    udev_list_entry_foreach (deventry, devlist)
    {
        const char *path = udev_list_entry_get_name (deventry);
        struct udev_device *dev = udev_device_new_from_syspath (udev, path);

        const char *version; 
        version = udev_device_get_property_value (dev, "ID_V4L_VERSION");
        /* we do not support video4linux 1 */
        if (version && strcmp (version, "1")) {
            const char *devpath = udev_device_get_devnode (dev);
            if (devpath) {
                try {
                    addDevice(devpath);
                } catch (const char *s) {
                    std::cerr << s << std::endl;
                }
            }
        }
        udev_device_unref (dev);
    }

    udev_monitor_unref (mon);
    udev_unref (udev);
    return;

udev_error:
    if (mon)
        udev_monitor_unref (mon);
    if (udev)
        udev_unref (udev);

#endif // HAVE_UDEV

    /* fallback : go through /dev/video* */
    int idx;
    for(idx = 0;;idx++) {
        std::stringstream ss;
        ss << "/dev/video" << idx;
        try {
            if (!addDevice(ss.str().c_str()))
                return;
        } catch (const char *s) {
            std::cerr << s << std::endl;
            return;
        }
    }
}

bool VideoV4l2List::addDevice(const char *dev) throw(const char *)
{
    int fd = open(dev, O_RDWR);
    if (fd == -1)
        return false;

    try {
        std::string s(dev);
        VideoV4l2Device v(fd, s);
        devices.push_back(v);
    }
    catch (const char *s) {
        close(fd);
        throw(s);
    }

    close(fd);
    return true;
}

void VideoV4l2List::setDevice(unsigned index)
{
    if (index >= devices.size())
        index = devices.size() - 1;

    _currentDevice = index;
}

std::vector<std::string> VideoV4l2List::getDeviceList(void)
{
    std::vector<std::string> v;
    std::stringstream ss;

    size_t n = devices.size();
    unsigned i;
    for (i = 0 ; i < n ; i++) {
        VideoV4l2Device &dev = devices[i];
        std::string &name = dev.name;
        if (name.length()) {
            ss << name;
        } else {
            ss << dev.device;
        }
        v.push_back(ss.str());
    }

    return v;
}

unsigned VideoV4l2List::getDeviceIndex()
{
    return _currentDevice;
}

VideoV4l2Device &VideoV4l2List::getDevice()
{
    return devices[_currentDevice];
}

} // namespace sfl_video
