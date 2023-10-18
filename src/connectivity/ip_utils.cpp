/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "ip_utils.h"

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
#define JAMI_DEVICE_SIGNAL 1
#endif

#if JAMI_DEVICE_SIGNAL
#include "client/ring_signal.h"
#endif

#include <dhtnet/ip_utils.h>

namespace jami {

std::string
ip_utils::getDeviceName()
{
#if JAMI_DEVICE_SIGNAL
    std::vector<std::string> deviceNames;
    emitSignal<libjami::ConfigurationSignal::GetDeviceName>(&deviceNames);
    if (not deviceNames.empty()) {
        return deviceNames[0];
    }
#endif
    return dhtnet::ip_utils::getHostname();
}

} // namespace jami
