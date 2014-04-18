/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include "sipcall.h"
#include "sip_utils.h"
#include "logger.h" // for _debug
#include "sdp.h"
#include "manager.h"
#ifdef SFL_VIDEO
#include "client/video_controls.h"
#endif

namespace {
    static const int INITIAL_SIZE = 16384;
    static const int INCREMENT_SIZE = INITIAL_SIZE;
}

SIPCall::SIPCall(const std::string& id, Call::CallType type,
        pj_caching_pool *caching_pool, const std::string &account_id) :
    Call(id, type, account_id)
    , inv(NULL)
    , audiortp_(this)
#ifdef SFL_VIDEO
    // The ID is used to associate video streams to calls
    , videortp_(id, Manager::instance().getClient()->getVideoControls()->getSettings())
#endif
    , pool_(pj_pool_create(&caching_pool->factory, id.c_str(), INITIAL_SIZE, INCREMENT_SIZE, NULL))
    , local_sdp_(new Sdp(pool_))
    , contactBuffer_()
    , contactHeader_{contactBuffer_, 0}
{}

SIPCall::~SIPCall()
{
    delete local_sdp_;
    pj_pool_release(pool_);
}

void SIPCall::setContactHeader(pj_str_t *contact)
{
    pj_strcpy(&contactHeader_, contact);
}

void SIPCall::answer()
{
    pjsip_tx_data *tdata;
    if (!inv->last_answer)
        throw std::runtime_error("Should only be called for initial answer");

    // answer with SDP if no SDP was given in initial invite (i.e. inv->neg is NULL)
    if (pjsip_inv_answer(inv, PJSIP_SC_OK, NULL, !inv->neg ? local_sdp_->getLocalSdpSession() : NULL, &tdata) != PJ_SUCCESS)
        throw std::runtime_error("Could not init invite request answer (200 OK)");

    // contactStr must stay in scope as long as tdata
    if (contactHeader_.slen) {
        DEBUG("Answering with contact header: %.*s", contactHeader_.slen, contactHeader_.ptr);
        sip_utils::addContactHeader(&contactHeader_, tdata);
    }

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        throw std::runtime_error("Could not send invite request answer (200 OK)");

    setConnectionState(CONNECTED);
    setState(ACTIVE);
}

std::map<std::string, std::string>
SIPCall::createHistoryEntry() const
{
    using sfl::HistoryItem;

    std::map<std::string, std::string> entry(Call::createHistoryEntry());
    return entry;
}
