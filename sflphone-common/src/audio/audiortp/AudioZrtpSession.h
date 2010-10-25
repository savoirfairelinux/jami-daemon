/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#ifndef __SFL_AUDIO_ZRTP_SESSION_H__
#define __SFL_AUDIO_ZRTP_SESSION_H__

#include <libzrtpcpp/zrtpccrtp.h>

#include "AudioRtpRecordHandler.h"

class ManagerImpl;
class SIPCall;

namespace sfl
{

class ZrtpZidException: public std::exception
{
        virtual const char* what() const throw() {
            return "ZRTP ZID initialization failed.";
        }
};

class AudioZrtpSession : public ost::TimerPort, public ost::SymmetricZRTPSession, public AudioRtpRecordHandler
{
    public:
        AudioZrtpSession (ManagerImpl * manager, SIPCall * sipcall, const std::string& zidFilename);

    private:
        void initializeZid (void);
        std::string _zidFilename;

        ost::Time * _time;

        // This semaphore is not used
        // but is needed in order to avoid
        // ambiguous compiling problem.
        // It is set to 0, and since it is
        // optional in ost::thread, then
        // it amounts to the same as doing
        // start() with no semaphore at all.
        ost::Semaphore * _mainloopSemaphore;

        // Main destination address for this rtp session.
        // Stored in case or reINVITE, which may require to forget
        // this destination and update a new one.
        ost::InetHostAddress _remote_ip;


        // Main destination port for this rtp session.
        // Stored in case reINVITE, which may require to forget
        // this destination and update a new one
        unsigned short _remote_port;

        /**
         * Manager instance.
         */
        ManagerImpl * _manager;

        /**
         * Timestamp for this session
         */
        int _timestamp;

        /**
         * Timestamp incrementation value based on codec period length (framesize)
         * except for G722 which require a 8 kHz incrementation.
         */
        int _timestampIncrement;

        /**
         * Timestamp reset freqeuncy specified in number of packet sent
         */
        short _timestampCount;

        /**
         * Time counter used to trigger incoming call notification
         */
        int _countNotificationTime;

        SIPCall * _ca;
};

}

#endif // __AUDIO_ZRTP_SESSION_H__
