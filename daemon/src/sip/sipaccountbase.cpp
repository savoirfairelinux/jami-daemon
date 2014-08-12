/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
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

#include "sipaccountbase.h"
#include "sipvoiplink.h"

bool SIPAccountBase::portsInUse_[HALF_MAX_PORT];

SIPAccountBase::SIPAccountBase(const std::string& accountID)
    : Account(accountID), link_(getSIPVoIPLink())
{}


void
SIPAccountBase::setTransport(pjsip_transport* transport, pjsip_tpfactory* lis)
{
    // release old transport
    if (transport_ && transport_ != transport) {
        pjsip_transport_dec_ref(transport_);
    }
    if (tlsListener_ && tlsListener_ != lis)
        tlsListener_->destroy(tlsListener_);
    // set new transport
    transport_ = transport;
    tlsListener_ = lis;
}

// returns even number in range [lower, upper]
uint16_t
SIPAccountBase::getRandomEvenNumber(const std::pair<uint16_t, uint16_t> &range)
{
    const uint16_t halfUpper = range.second * 0.5;
    const uint16_t halfLower = range.first * 0.5;
    uint16_t result;
    do {
        result = 2 * (halfLower + rand() % (halfUpper - halfLower + 1));
    } while (portsInUse_[result / 2]);

    portsInUse_[result / 2] = true;
    return result;
}

void
SIPAccountBase::releasePort(uint16_t port)
{
    portsInUse_[port / 2] = false;
}

uint16_t
SIPAccountBase::generateAudioPort() const
{
    return getRandomEvenNumber(audioPortRange_);
}

#ifdef SFL_VIDEO
uint16_t
SIPAccountBase::generateVideoPort() const
{
    return getRandomEvenNumber(videoPortRange_);
}
#endif
