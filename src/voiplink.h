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
#include "contact/contact.h"
#include <map>
#include <cc++/thread.h> // for mutex

class AudioCodec;

//#include "account.h" // for AccountID
// replaced by:
typedef std::string AccountID;

typedef std::map<CallID, Call*> CallMap;

/**
 * Listener and manager interface for each VoIP protocol
 */
class VoIPLink {
public:
  VoIPLink(const AccountID& accountID);
  virtual ~VoIPLink (void);

  enum RegistrationState {Unregistered, Trying, Registered, Error};

  // Pure virtual functions
  virtual void getEvent (void) = 0;
  virtual bool init (void) = 0;
  virtual bool checkNetwork (void) = 0;
  virtual void terminate (void) = 0;
  virtual bool sendRegister (void) = 0;
  virtual bool sendUnregister (void) = 0;

  /** Add a new outgoing call and return the call pointer or 0 if and error occurs */
  virtual Call* newOutgoingCall(const CallID& id, const std::string& toUrl) = 0;
  virtual bool answer(const CallID& id) = 0;

  /**
   * Hang up a call
   */
  virtual bool hangup(const CallID& id) = 0;

  /**
   * Cancel the call dialing
   */
  virtual bool cancel(const CallID& id) = 0;

  /**
   * Put a call on hold
   */
  virtual bool onhold(const CallID& id) = 0;

  /**
   * Resume a call from hold state
   */
  virtual bool offhold(const CallID& id) = 0;

  /**
   * Transfer a call to specified URI
   */
  virtual bool transfer(const CallID& id, const std::string& to) = 0;

  /**
   * Refuse incoming call
   */
  virtual bool refuse(const CallID& id) = 0;

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
  virtual void subscribePresenceForContact(Contact* contact);
  
  /**
   * Publish presence status to server
   */
  virtual void publishPresenceStatus(std::string status);
  
  // these method are set only with 'Account init'  and can be get by everyone
  void setFullName (const std::string& fullname) { _fullname = fullname; }
  std::string& getFullName (void) { return _fullname; }
  void setHostName (const std::string& hostname) {  _hostname = hostname; }
  std::string& getHostName (void) { return _hostname; }

  /**
   * Return parent Account's ID
   */
  AccountID& getAccountID(void) { return _accountID; }

  /** Get the call pointer from the call map (protected by mutex)
   * @param id A Call ID
   * @return call pointer or 0
   */
  Call* getCall(const CallID& id);

  /**
   * Get registration state
   */
  enum RegistrationState getRegistrationState() { return _registrationState; }

  /**
   * Get registration error message, if set.
   */
  std::string getRegistrationError() { return _registrationError; }

  /**
   * Set new registration state
   *
   * We use this function, in case the server needs to PUSH to the
   * GUI when the state changes.
   */
  void setRegistrationState(const enum RegistrationState state,
			    const std::string& errorMessage);

  /**
   * Same, but with default error value to ""
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
   * Registration error message
   */
  std::string _registrationError;

protected:
  /** Add a call to the call map (protected by mutex)
   * @param call A call pointer with a unique pointer
   * @return true if the call was unique and added
   */
  bool addCall(Call* call);

  /** Remove a call from the call map (protected by mutex)
   * @param id A Call ID
   * @return true if the call was correctly removed
   */
  bool removeCall(const CallID& id);

  /**
   * Remove all the call from the map
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
   *
   * This should be used in [IAX|SIP]VoIPLink::init() and terminate(), to
   * indicate that init() was called, or reset by terminate().
   */
  bool _initDone;
};

#endif // __VOIP_LINK_H__
