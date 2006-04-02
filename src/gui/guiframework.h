/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#ifndef __GUI_FRAMEWORK_H__
#define __GUI_FRAMEWORK_H__
/* Inherited class by GUI classes */
/* The GuiFramework class is the base of all user interface */

#include <string>
#include "server/argtokenizer.h"
#include "../observer.h"
#include "../call.h"   // for callid2
#include "../account.h" // for account

class GuiFramework {
public:
	GuiFramework ();
	virtual ~GuiFramework (void);

	/* Parent class to child class */
  virtual bool incomingCall (const AccountID& accountId, const CallID& id, const std::string& from) = 0;

	virtual void peerAnsweredCall (const CallID& id) = 0;
	virtual void peerRingingCall (const CallID& id) = 0;
	virtual void peerHungupCall (const CallID& id) = 0;
  virtual void incomingMessage(const AccountID& accountId, const std::string& message) = 0;
	virtual void displayStatus (const std::string& status) = 0;
	virtual void displayConfigError (const std::string& error) = 0;
	virtual void displayTextMessage (const CallID& id, const std::string& message) = 0;
	virtual void displayErrorText (const CallID& id, const std::string& message) = 0;
	virtual void displayError (const std::string& error) = 0;
	virtual void startVoiceMessageNotification (void) {}
	virtual void stopVoiceMessageNotification (void) {}
  virtual void sendVoiceNbMessage(const AccountID& accountId, const std::string& nb_msg) = 0;
	virtual void setup() = 0;
  virtual void sendMessage(const std::string& code, const std::string& seqId, TokenList& arg) = 0;
  virtual void sendCallMessage(const std::string& code, 
  const std::string& sequenceId, const CallID& id, TokenList arg) = 0;
  virtual void sendRegistrationState(const AccountID& accountid, bool state) = 0;
  virtual void callFailure(const CallID& id) = 0;

	/* Child class to parent class */
  bool outgoingCall(const AccountID& account, const CallID& id, const std::string& to);
  bool answerCall(const CallID& id);

  bool sendTextMessage(const AccountID& accountId, const std::string& to, const std::string& message);
	bool hangupCall (const CallID& id);
	bool cancelCall (const CallID& id);
	bool onHoldCall (const CallID& id);
	bool offHoldCall (const CallID& id);
	bool transferCall (const CallID& id, const std::string& to);
	void mute ();
	void unmute ();
	bool refuseCall (const CallID& id);

  bool saveConfig(void);
  bool registerVoIPLink(const AccountID& accountId);
  bool unregisterVoIPLink(const AccountID& accountId);
  bool sendDtmf (const CallID& id, char code);
  bool playDtmf (char code);
  bool playTone ();
  bool stopTone ();

  // config
  bool getEvents();
  bool getZeroconf(const std::string& sequenceId);
  bool attachZeroconfEvents(const std::string& sequenceId, Pattern::Observer& observer);
  bool detachZeroconfEvents(Pattern::Observer& observer);
  bool getCallStatus(const std::string& sequenceId);
  bool getConfigAll(const std::string& sequenceId);
  bool getConfig(const std::string& section,  const std::string& name, TokenList& arg);
  bool setConfig(const std::string& section, const std::string& name, const std::string& value);
  bool getConfigList(const std::string& sequenceId, const std::string& name);
  bool setSpkrVolume(int volume);
  bool setMicVolume(int volume);
  int getSpkrVolume();
  int getMicVolume();
  bool setSwitch(const std::string& switchName);

  bool hasLoadedSetup();
  const CallID& getCurrentId();
  bool getRegistrationState(std::string& stateCode, std::string& stateMessage);

protected:
  std::string _message;
};

#endif // __GUI_FRAMEWORK_H__
