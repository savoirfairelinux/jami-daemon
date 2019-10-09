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

#include "logger.h"
#include "media/remb.h"

#include <cstdint>
#include <utility>


static constexpr uint8_t packetVersion = 2;
static constexpr uint8_t packetFMT = 15;
static constexpr uint8_t packetType = 206;
static constexpr uint32_t uniqueIdentifier = 0x52454D42;  // 'R' 'E' 'M' 'B'.


namespace jami { namespace video {
// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| FMT=15  |   PT=206      |             length            |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  0 |                  SSRC of packet sender                        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                       Unused = 0                              |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |  Unique identifier 'R' 'E' 'M' 'B'                            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |  Num SSRC     | BR Exp    |  BR Mantissa                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |   SSRC feedback                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    :  ...                                                          :

Remb::Remb()
{}

Remb::~Remb()
{}

static void
insert2Byte(std::vector<uint8_t> *v, uint16_t val)
{
    v->insert(v->end(), val >> 8);
    v->insert(v->end(), val & 0xff);
}

static void
insert4Byte(std::vector<uint8_t> *v, uint32_t val)
{
    v->insert(v->end(), val >> 24);
    v->insert(v->end(), (val >> 16) & 0x00ff);
    v->insert(v->end(), (val >> 8) & 0x0000ff);
    v->insert(v->end(), val & 0x000000ff);
}


uint64_t
Remb::parseREMB(rtcpREMBHeader* packet)
{
    if(packet->fmt != 15 || packet->pt != 206) {
        JAMI_ERR("Unable to parse REMB packet.");
        return 0;
    }

    uint64_t bitrate_bps = (packet->br_mantis << packet->br_exp);

    bool shift_overflow = (bitrate_bps >> packet->br_exp) != packet->br_mantis;
    if (shift_overflow) {
        JAMI_ERR("Invalid remb bitrate value : %u*2^%u", packet->br_mantis, packet->br_exp);
        return false;
    }

    return bitrate_bps;
}

std::vector<uint8_t>
Remb::createREMB(uint64_t bitrate_bps) {
    std::vector<uint8_t> remb;

    remb.insert(remb.end(), packetVersion << 4 | packetFMT);
    remb.insert(remb.end(), packetType);
    insert2Byte(&remb, 5); // (sizeof(rtcpREMBHeader)/4)-1 -> not safe
    insert4Byte(&remb, 0x12345678);     //ssrc
    insert4Byte(&remb, 0x0);     //ssrc source
    insert4Byte(&remb, uniqueIdentifier);     //uid
    remb.insert(remb.end(), 1);                     //n_ssrc

    const uint32_t maxMantissa = 0x3ffff;  // 18 bits.
    uint64_t mantissa = bitrate_bps;
    uint8_t exponenta = 0;
    while (mantissa > maxMantissa) {
        mantissa >>= 1;
        ++exponenta;
    }

    remb.insert(remb.end(), (exponenta << 2) | (mantissa >> 16));
    insert2Byte(&remb, mantissa & 0xffff);
    insert4Byte(&remb, 0x2345678b);

    return std::move(remb);
}

}} // namespace jami::video