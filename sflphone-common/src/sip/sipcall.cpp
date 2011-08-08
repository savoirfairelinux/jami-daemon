/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "sipcall.h"
#include "logger.h" // for _debug
#include "audio/audiortp/AudioRtpFactory.h"
#include "video/video_rtp_session.h"
#include "sdp.h"

const int SIPCall::CALL_MEMPOOL_INIT_SIZE = 16384;
const int SIPCall::CALL_MEMPOOL_INC_SIZE = 16384;   // Must be large enough to clone sdp sessions

SIPCall::SIPCall (const std::string& id, Call::CallType type, pj_caching_pool *caching_pool) : Call (id, type)
    , _cid (0)
    , _did (0)
    , _tid (0)
    , _audiortp (new sfl::AudioRtpFactory(this))
    , videortp_ (new sfl_video::VideoRtpSession)
    , _xferSub (NULL)
    , _invSession (NULL)
    , _local_sdp (NULL)
	, _pool(NULL)
{
    _debug ("SIPCall: Create new call %s", id.c_str());

    // Create memory pool for application, initialization value is based on empiric values.
    _pool = pj_pool_create (&caching_pool->factory, id.c_str(), CALL_MEMPOOL_INIT_SIZE,
                            CALL_MEMPOOL_INC_SIZE, NULL);

    _local_sdp = new Sdp (_pool);
}

SIPCall::~SIPCall()
{
    _debug ("SIPCall: Delete call");

    delete _audiortp;
    _audiortp = NULL;

    delete _local_sdp;
    _local_sdp = NULL;

    _debug ("SDP: pool capacity %d", pj_pool_get_capacity (_pool));
    _debug ("SDP: pool size %d", pj_pool_get_used_size (_pool));

    // Release memory allocated for SDP
    pj_pool_release (_pool);
    _pool = NULL;

}
