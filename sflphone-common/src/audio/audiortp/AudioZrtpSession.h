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
#ifndef __SFL_AUDIO_ZRTP_SESSION_H__
#define __SFL_AUDIO_ZRTP_SESSION_H__

#include <libzrtpcpp/zrtpccrtp.h>

#include "AudioRtpSession.h"

class ManagerImpl;
class SIPCall;

namespace sfl {

    class ZrtpZidException: public std::exception
    {
        virtual const char* what() const throw()
        {
        return "ZRTP ZID initialization failed.";
        }
    };

    class AudioZrtpSession : public ost::SymmetricZRTPSession, public AudioRtpSession<AudioZrtpSession> 
    {
        public:
        AudioZrtpSession(ManagerImpl * manager, SIPCall * sipcall, const std::string& zidFilename);          
            
        private:
            void initializeZid(void);
            std::string _zidFilename;
    };
   
}

#endif // __AUDIO_ZRTP_SESSION_H__
