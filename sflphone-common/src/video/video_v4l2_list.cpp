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

#include <iostream>
#include <sstream>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include "logger.h"
#include "video_v4l2_list.h"

namespace sfl_video {

VideoV4l2List::VideoV4l2List() : _currentDevice(0)
{
    int idx;
    for(idx = 0;;idx++) {

        std::stringstream ss;
        ss << "/dev/video" << idx;
        int fd = open(ss.str().c_str(), O_RDWR);
        if (fd == -1)
            break;

        try {
            std::string str(ss.str());
            VideoV4l2Device v(fd, str);
            devices.push_back(v);
        }
        catch (int e) {
            close(fd);
            break;
        }

        close(fd);
    }
}

void VideoV4l2List::setDevice(unsigned index)
{
    if (index >= devices.size()) {
        _error("%s: requested device %d but we only have %d", __PRETTY_FUNCTION__, index, devices.size());
        index = devices.size() - 1;
    }
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
