/*
 *  Copyright (C) 2011-2016 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "video_rtp_session.h"
#include "video_device_monitor.h"

#include "ip_utils.h"

#include <unistd.h> // for sleep

int main ()
{
    ring::video::VideoDeviceMonitor monitor;
    ring::video::VideoRtpSession session("test", monitor.getDeviceParams(monitor.getDefaultDevice()));

    ring::MediaDescription local {};
    local.addr = {AF_INET};
    local.addr.setPort(12345);
    session.updateMedia(local, ring::MediaDescription{});

    session.start();
    sleep(5);
    session.stop();

    return 0;
}
