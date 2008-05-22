/*
 *  Copyright (C) 2005-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 */

#ifndef __VOIP_LINK_H__
#define __VOIP_LINK_H__

#include <string>
#include "call.h"
#include <map>
#include <cc++/thread.h> // for mutex

class AudioCodec;

/** Define AccountID type */
typedef std::string AccountID;

/** Define a map that associate a Call object to a call identifier */
typedef std::map<CallID, Call*> CallMap;

/**
 * @file voiplink.h
 * @brief Listener and manager interface for each VoIP protocol
 */
class VoIPLink {
  public:

    /**
     * Constructor
     * @param accountID The account identifier
     */
    VoIPLink(const AccountID& accountID);

    /**
     * Virtual destructor
     */
    virtual ~VoIPLink (void);

    /** Contains all the state an Voip can be in */
    enum RegistrationState {Unregistered, Trying, Registered, Error, ErrorAuth , ErrorNetwork , ErrorHost};

    /**
     * Virtual method
     * Event listener. Each event send by the call manager is received and handled from here
     */
    virtual void getEvent (void) = 0;

    /** 
     * Virtual method
     * Try to initiate the eXosip engine/thread and set config 
     * @return bool True if OK
     */
    virtual bool init (void) = 0;
    
    /**
     * Virtual method
     * Check if a local IP can be found
     * @return bool True if network is reachable
     */
    virtual bool checkNetwork (void) = 0;
    
    /**
     * Virtual method
     * Delete link-related stuuf like calls
     */
    virtual void terminate (void) = 0;
    
    /**
     * Virtual method
     * Build and send SIP registration request
     * @return bool True on success
     *		  false otherwise
     */
    virtual bool sendRegister (void) = 0;
    
    /**
     * Virtual method
     * Build and send SIP unregistration request
     * @return bool True on success
     *		  false otherwise
     */
    virtual bool sendUnregister (void) = 0;

    /**
     * Place a new call
     * @param id  The call identifier
     * @param toUrl  The Sip address of the recipient of the call
     * @return Call* The current call
     */
    virtual Call* newOutgoingCall(const CallID& id, const std::string& toUrl) = 0;
    /**
     * Answer the call
     * @param id The call identifier
     * @return bool True on success
     */
    virtual bool answer(const CallID& id) = 0;

    /**
     * Hang up a call
     * @param id The call identifier
     * @return bool True on success
     */
    virtual bool hangup(const CallID& id) = 0;

    /**
     * Cancel the call dialing
     * @param id The call identifier
     * @return bool True on success
     */
    virtual bool cancel(const CallID& id) = 0;

    /**
     * Put a call on hold
     * @param id The call identifier
     * @return bool True on success
     */
    virtual bool onhold(const CallID& id) = 0;

    /**
     * Resume a call from hold state
     * @param id The call identifier
     * @return bool True on success
     */
    virtual bool offhold(const CallID& id) = 0;

    /**
     * Transfer a call to specified URI
     * @param id The call identifier
     * @param to The recipient of the call
     * @return bool True on success
     */
    virtual bool transfer(const CallID& id, const std::string& to) = 0;

    /**
     * Refuse incoming call
     * @param id The call identifier
     * @return bool True on success
     */
    virtual bool refuse(const CallID& id) = 0;

    /**
     * Send DTMF
     * @param id The call identifier
     * @param code  The char code
     * @return bool True on success
     */
    virtual bool carryingDTMFdigits(const CallID& id, char code) = 0;

    /**
     * Send text message
     */
    virtual bool sendMessage(const std::string& to, const std::string& body) = 0;

    // NOW
    /**
     * Determine if link supports presence information
     */
    virtual bool isContactPresenceSupported() = 0;

    /**
     * Register contacts for presence information if supported
     */
    //virtual void subscribePresenceForContact(Contact* contact);

    /**
     * Publish presence status to server
     */
    virtual void publishPresenceStatus(std::string status);

    /**
     * Set the account full name
     * @param fullname	The full name
     */
    void setFullName (const std::string& fullname) { _fullname = fullname; }

    /**
     * Get the account full name
     * @return std::string The full name
     */
    std::string& getFullName (void) { return _fullname; }

    /**
     * Set the account host name
     * @param hostname	The host name
     */
    void setHostName (const std::string& hostname) {  _hostname = hostname; }
    
    /**
     * Get the account host name
     * @return std::string  The host name
     */
    std::string& getHostName (void) { return _hostname; }

    /**
     * @return AccountID  parent Account's ID
     */
    AccountID& getAccountID(void) { return _accountID; }

    /**
     * @param accountID The account identifier
     */
    void setAccountID( const AccountID& accountID) { _accountID = accountID; }

    /** Get the call pointer from the call map (protected by mutex)
     * @param id A Call ID
     * @return Call*  Call pointer or 0
     */
    Call* getCall(const CallID& id);

    /**
     * Get registration state
     * @return RegistrationState
     */
    enum RegistrationState getRegistrationState() { return _registrationState; }

    /**
     * Get registration error message, if set.
     */
    int getRegistrationError() { return _registrationError; }

    /**
     * Set new registration state
     * We use this function, in case the server needs to PUSH to the
     * GUI when the state changes.
     * @param state The registration state
     * @param errorCode The error code
     */
    void setRegistrationState(const enum RegistrationState state,
	const int& errorCode);

    /**
     * Set new registration state
     * @param state The registration state
     */
    void setRegistrationState(const enum RegistrationState state);

  private:
    /**
     * Full name used as outgoing Caller ID
     */
    std::string _fullname;

    /**
     * Host name used for authentication
     */
    std::string _hostname;

    /**
     * ID of parent's Account
     */
    AccountID _accountID;

    /**
     * State of registration
     */
    enum RegistrationState _registrationState;

    /**
     * Registration error code -> refers to global.h
     */
    int _registrationError;

  protected:
    /** Add a call to the call map (protected by mutex)
     * @param call A call pointer with a unique pointer
     * @return bool True if the call was unique and added
     */
    bool addCall(Call* call);

    /** Remove a call from the call map (protected by mutex)
     * @param id A Call ID
     * @return bool True if the call was correctly removed
     */
    bool removeCall(const CallID& id);

    /**
     * Remove all the call from the map
     * @return bool True on success
     */
    bool clearCallMap();

    /** Contains all the calls for this Link, protected by mutex */
    CallMap _callMap;

    /** Mutex to protect call map */
    ost::Mutex _callMapMutex;

    /** Get Local IP Address (ie: 127.0.0.1, 192.168.0.1, ...) */
    std::string _localIPAddress;

    /** Get local listening port (5060 for SIP, ...) */
    unsigned int _localPort;

    /** Whether init() was called already or not
     * This should be used in [IAX|SIP]VoIPLink::init() and terminate(), to
     * indicate that init() was called, or reset by terminate().
     */
    bool _initDone;
};

#endif // __VOIP_LINK_H__
