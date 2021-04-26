/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <map>
#include "pjsip-ua/sip_inv.h"

namespace jami {

class SipTransport;

// Interface used by SipVoipLink to report SIP events.
// This interface is meant to be used only within SIP module. The main use
// of this interface is to give only a limited access to SIPCall to prevent
// side effects.
class SipEventsHandler
{
public:
    virtual ~SipEventsHandler() {};

    virtual void onReceivedCall(const std::string& peerNumber,
                                pjmedia_sdp_session* remoteSdp,
                                pjsip_rx_data* rdata,
                                const std::shared_ptr<SipTransport>& sipTr = {})
        = 0;
    virtual void onSdpMediaUpdate(pjsip_inv_session* inv, pj_status_t status) = 0;
    virtual void onSdpCreateOffer(pjsip_inv_session* inv, pjmedia_sdp_session** p_offer) = 0;
    virtual void onInviteSessionStateChanged(pjsip_inv_session* inv, pjsip_event* ev) = 0;

    /**
     * Report a new offer from peer on a existing invite session
     * (aka re-invite)
     */
    virtual pj_status_t onReceiveReinvite(const pjmedia_sdp_session* offer, pjsip_rx_data* rdata) = 0;

    virtual void onReceivedTextMessage(std::map<std::string, std::string> messages) = 0;

    virtual void onRequestInfo(pjsip_rx_data* rdata, pjsip_msg* msg) = 0;
    virtual void onRequestRefer(pjsip_inv_session* inv, pjsip_rx_data* rdata, pjsip_msg* msg) = 0;
    virtual void onRequestNotify(pjsip_inv_session* inv, pjsip_rx_data* rdata, pjsip_msg* msg) = 0;
};
} // namespace jami