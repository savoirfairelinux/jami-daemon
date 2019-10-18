/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Author: Pierre Lespagnol <pierre.lespagnol@savoirfairelinux.com>
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

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_

#include <vector>
#include <cstdint>

#include "socket_pair.h"

namespace jami { namespace video {

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
class Remb {
public:
    Remb();
    ~Remb();

    static uint64_t parseREMB(rtcpREMBHeader* packet);
    std::vector<uint8_t> createREMB(uint64_t bitrate_bps);

private:

};

}} // namespace jami::video
#endif
