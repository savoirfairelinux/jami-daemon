/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __AUDIO_SYMMETRIC_RTP_SESSION_H__
#define __AUDIO_SYMMETRIC_RTP_SESSION_H__

#include <ccrtp/rtp.h>

#include "AudioRtpSession.h"

namespace sfl {
    class AudioSymmetricRtpSession : public ost::SymmetricRTPSession, public AudioRtpSession<AudioSymmetricRtpSession> 
    {
        public:
        AudioSymmetricRtpSession(SIPCall * sipcall) :
            ost::SymmetricRTPSession(ost::InetHostAddress(sipcall->getLocalIp().c_str()), sipcall->getLocalAudioPort()),
            AudioRtpSession<AudioSymmetricRtpSession>(sipcall)
        {
            _debug("AudioSymmetricRtpSession initialized\n");
        }       
    };
}

#endif // __AUDIO_SYMMETRIC_RTP_SESSION_H__
