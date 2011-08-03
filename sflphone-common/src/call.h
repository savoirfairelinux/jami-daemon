/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
#ifndef CALL_H
#define CALL_H

#include <cc++/thread.h> // for mutex
#include <sstream>

#include "audio/recordable.h"

#define SIP_SCHEME       "sip:"
#define SIPS_SCHEME      "sips:"

#define CallConfigNULL   0

/*
 * @file call.h
 * @brief A call is the base class for protocol-based calls
 */

class Call: public Recordable
{
    public:

        /**
         * This determines if the call is a direct IP-to-IP call or a classic call, made with an existing account
         */
        enum CallConfiguration {Classic, IPtoIP};

        /**
         * This determines if the call originated from the local user (Outgoing)
         * or from some remote peer (Incoming).
         */
        enum CallType {Incoming, Outgoing};

        /**
         * Tell where we're at with the call. The call gets Connected when we know
         * from the other end what happened with out call. A call can be 'Connected'
         * even if the call state is Busy, Refused, or Error.
         *
         * Audio should be transmitted when ConnectionState = Connected AND
         * CallState = Active.
         */
        enum ConnectionState {Disconnected, Trying, Progressing, Ringing, Connected};

        /**
         * The Call State.
         */
        enum CallState {Inactive, Active, Hold, Busy, Conferencing, Refused, Error};

        /**
         * Constructor of a call
         * @param id Unique identifier of the call
         * @param type set definitely this call as incoming/outgoing
         */
        Call (const std::string& id, Call::CallType type);
        virtual ~Call();

        /**
         * Return a reference on the call id
         * @return call id
         */
        std::string& getCallId() {
            return _id;
        }

        /**
             * Return a reference on the conference id
             * @return call id
             */
        const std::string& getConfId() const {
            return _confID;
        }

        void setConfId (std::string id) {
            _confID = id;
        }

        CallType getCallType (void) const {
            return _type;
        }

        /**
         * Set the peer number (destination on outgoing)
         * not protected by mutex (when created)
         * @param number peer number
         */
        void setPeerNumber (const std::string& number) {
            _peerNumber = number;
        }

        /**
         * Get the peer number (destination on outgoing)
         * not protected by mutex (when created)
         * @return std::string The peer number
         */
        const std::string& getPeerNumber() const {
            return _peerNumber;
        }

        /**
         * Set the peer name (caller in ingoing)
         * not protected by mutex (when created)
         * @param name The peer name
         */
        void setPeerName (const std::string& name) {
            _peerName = name;
        }

        /**
         * Get the peer name (caller in ingoing)
         * not protected by mutex (when created)
         * @return std::string The peer name
         */
        const std::string& getPeerName() const {
            return _peerName;
        }

        /**
             * Set the display name (caller in ingoing)
             * not protected by mutex (when created)
             * @return std::string The peer display name
             */
        void setDisplayName (const std::string& name) {
            _displayName = name;
        }

        /**
             * Get the peer display name (caller in ingoing)
             * not protected by mutex (when created)
             * @return std::string The peer name
             */
        const std::string& getDisplayName() const {
            return _displayName;
        }

        /**
         * Tell if the call is incoming
         * @return true if yes
         *	      false otherwise
         */
        bool isIncoming() {
            return (_type == Incoming) ? true : false;
        }

        /**
         * Set the connection state of the call (protected by mutex)
         * @param state The connection state
         */
        void setConnectionState (ConnectionState state);

        /**
         * Get the connection state of the call (protected by mutex)
         * @return ConnectionState The connection state
         */
        ConnectionState getConnectionState();

        /**
         * Set the state of the call (protected by mutex)
         * @param state The call state
         */
        void setState (CallState state);

        /**
         * Get the call state of the call (protected by mutex)
         * @return CallState  The call state
         */
        CallState getState();

        std::string getStateStr ();

        void setCallConfiguration (Call::CallConfiguration callConfig) {
            _callConfig = callConfig;
        }

        Call::CallConfiguration getCallConfiguration (void) const {
            return _callConfig;
        }

        /**
         * Set my IP [not protected]
         * @param ip  The local IP address
         */
        void setLocalIp (const std::string& ip)     {
            _localIPAddress = ip;
        }

        /**
         * Set local audio port, as seen by me [not protected]
         * @param port  The local audio port
         */
        void setLocalAudioPort (unsigned int port)  {
            _localAudioPort = port;
        }

        /**
         * Return my IP [mutex protected]
         * @return std::string The local IP
         */
        const std::string& getLocalIp();

        /**
         * Return port used locally (for my machine) [mutex protected]
         * @return unsigned int  The local audio port
         */
        unsigned int getLocalAudioPort();

        std::string getRecFileId (void) const {
            return getPeerName();
        }

        std::string getFileName (void) const {
            return _peerNumber;
        }

        virtual bool setRecording (void);

    private:
        /** Protect every attribute that can be changed by two threads */
        ost::Mutex _callMutex;

        // Informations about call socket / audio

        /** My IP address */
        std::string  _localIPAddress;

        /** Local audio port, as seen by me. */
        unsigned int _localAudioPort;

        /** Unique ID of the call */
        std::string _id;

        /** Unique conference ID, used exclusively in case of a conferece */
        std::string _confID;

        /** Type of the call */
        CallType _type;

        /** Disconnected/Progressing/Trying/Ringing/Connected */
        ConnectionState _connectionState;

        /** Inactive/Active/Hold/Busy/Refused/Error */
        CallState _callState;

        /** Direct IP-to-IP or classic call */
        CallConfiguration _callConfig;

        /** Name of the peer */
        std::string _peerName;

        /** Number of the peer */
        std::string _peerNumber;

        /** Display Name */
        std::string _displayName;

        /** File name for his call, should be peer number */
        std::string _filename;
};

#endif
