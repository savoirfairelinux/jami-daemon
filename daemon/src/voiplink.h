/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#ifndef __VOIP_LINK_H__
#define __VOIP_LINK_H__

#include <stdexcept>

#include "call.h"

class Account;

/** Define a map that associate a Call object to a call identifier */
typedef std::map<std::string, Call*> CallMap;

class VoipLinkException : public std::runtime_error
{
    public:
        VoipLinkException (const std::string& str="") :
        	std::runtime_error("UserAgent: VoipLinkException occured: " + str) {}
};

/**
 * @file voiplink.h
 * @brief Listener and manager interface for each VoIP protocol
 */
class VoIPLink
{
    public:
        /**
         * Constructor
         */
        VoIPLink ();

        /**
         * Virtual destructor
         */
        virtual ~VoIPLink (void);


        /**
         * Virtual method
         * Event listener. Each event send by the call manager is received and handled from here
         */
        virtual void getEvent (void) = 0;

        /**
         * Virtual method
         * Try to initiate the communication layer and set config
         * @return bool True if OK
         */
        virtual bool init (void) = 0;

        /**
         * Virtual method
         * Delete link-related stuff like calls
         */
        virtual void terminate (void) = 0;

        /**
         * Virtual method
         * Build and send account registration request
         * @return bool True on success
         *		  false otherwise
         */
        virtual void sendRegister (std::string id) throw (VoipLinkException) = 0;

        /**
         * Virtual method
         * Build and send account unregistration request
         * @return bool True on success
         *		  false otherwise
         */
        virtual void sendUnregister (std::string id) throw (VoipLinkException) = 0;

        /**
         * Place a new call
         * @param id  The call identifier
         * @param toUrl  The address of the recipient of the call
         * @return Call* The current call
         */
        virtual Call* newOutgoingCall (const std::string& id, const std::string& toUrl) throw (VoipLinkException) = 0;

        /**
         * Answer the call
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool answer (const std::string& id) throw (VoipLinkException) = 0;

        /**
         * Hang up a call
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool hangup (const std::string& id)  throw (VoipLinkException) = 0;

        /**
        * Peer Hung up a call
        * @param id The call identifier
        * @return bool True on success
        */
        virtual bool peerHungup (const std::string& id) throw (VoipLinkException) = 0;

        /**
         * Cancel the call dialing
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool cancel (const std::string& id) throw (VoipLinkException) = 0;

        /**
         * Put a call on hold
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool onhold (const std::string& id) throw (VoipLinkException) = 0;

        /**
         * Resume a call from hold state
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool offhold (const std::string& id) throw (VoipLinkException) = 0;

        /**
         * Transfer a call to specified URI
         * @param id The call identifier
         * @param to The recipient of the call
         * @return bool True on success
         */
        virtual bool transfer (const std::string& id, const std::string& to) throw (VoipLinkException) = 0;

        /**
         * Attended transfer
         * @param The transfered call id
         * @param The target call id
         * @return True on success
         */
        virtual bool attendedTransfer (const std::string&, const std::string&) = 0;

        /**
         * Refuse incoming call
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool refuse (const std::string& id) = 0;

        /**
         * Send DTMF
         * @param id The call identifier
         * @param code  The char code
         * @return bool True on success
         */
        virtual bool carryingDTMFdigits (const std::string& id, char code) = 0;

        /**
         * Return the codec protocol used for this call
         * @param call The call
         */
        virtual std::string getCurrentVideoCodecName(const std::string& id) = 0;
        virtual std::string getCurrentCodecName(Call *call) = 0;

        bool initDone (void) {
            return _initDone;
        }
        void initDone (bool state) {
            _initDone = state;
        }

        /** Add a call to the call map (protected by mutex)
         * @param call A call pointer with a unique pointer
         * @return bool True if the call was unique and added
         */
        bool addCall (Call* call);

        /** Remove a call from the call map (protected by mutex)
         * @param id A Call ID
         */
        void removeCall (const std::string& id);

        /**
         * Remove all the call from the map
         * @return bool True on success
         */
        bool clearCallMap();

        /**
         * Get the call pointer from the call map (protected by mutex)
         * @param id A Call ID
         * @return Call*  Call pointer or 0
         */
        Call* getCall (const std::string& id);

    protected:
        /** Contains all the calls for this Link, protected by mutex */
        CallMap _callMap;

        /** Mutex to protect call map */
        ost::Mutex _callMapMutex;

        /** Get local listening port (5060 for SIP, ...) */
        unsigned int _localPort;

        /** Whether init() was called already or not
         * This should be used in [IAX|SIP]VoIPLink::init() and terminate(), to
         * indicate that init() was called, or reset by terminate().
         */
        bool _initDone;
};

#endif // __VOIP_LINK_H__
