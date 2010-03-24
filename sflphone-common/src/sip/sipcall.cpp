/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
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
 */

#include "sipcall.h"
#include "global.h" // for _debug
#include "sdp.h"

SIPCall::SIPCall (const CallID& id, Call::CallType type, pj_pool_t *pool) : Call (id, type)
        , _cid (0)
        , _did (0)
        , _tid (0)
        , _audiortp (new sfl::AudioRtpFactory())
        , _xferSub (NULL)
        , _invSession (NULL)
        , _local_sdp (0)
{
	_debug ("SIPCall: Create new call %s", id.c_str());

    _local_sdp = new Sdp (pool);
}

SIPCall::~SIPCall()
{
	_debug ("SIPCall: Delete call");

    delete _audiortp;
    _audiortp = 0;
    delete _local_sdp;
    _local_sdp = 0;
}



