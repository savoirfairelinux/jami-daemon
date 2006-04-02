/*
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
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
 */
#ifndef __GUI_SERVER_H__
#define __GUI_SERVER_H__

#include "../guiframework.h"
#include <string>
#include <map>

#include "requestmanager.h"


/** Session port for the daemon, default is DEFAULT_SESSION_PORT */
#define DEFAULT_SESSION_PORT 3999

typedef std::map<CallID, std::string> CallMap;

class GUIServerImpl : public GuiFramework {
public:
  // GUIServerImpl constructor
  GUIServerImpl();
  // GUIServerImpl destructor
  ~GUIServerImpl();
  
  // exec loop
  int exec(void);

  bool incomingCall(const AccountID& accountId, const CallID& id, const std::string& from);
  void incomingMessage(const AccountID& accountId, const std::string& message);

	void peerAnsweredCall (const CallID& id);
	void peerRingingCall (const CallID& id);
	void peerHungupCall (const CallID& id);
	void displayStatus (const std::string& status);
  void displayConfigError(const std::string& error);
	void displayTextMessage (const CallID& id, const std::string& message);
	void displayErrorText (const CallID& id, const std::string& message);
	void displayError (const std::string& error);
  void sendVoiceNbMessage(const AccountID& accountid, const std::string& nb_msg);
  void sendRegistrationState(const AccountID& accountid, bool state);
  void setup();

  void sendMessage(const std::string& code, const std::string& seqId, 
    TokenList& arg);
  void sendCallMessage(const std::string& code, const std::string& sequenceId, 
    const CallID& id, TokenList arg);
  void callFailure(const CallID& id);

  bool getEvents(const std::string& sequenceId);
  bool sendGetEventsEnd();

  bool outgoingCall (const std::string& seq, const std::string& account,
    const std::string& callid, const std::string& to);
  bool answerCall(const std::string& callId);
  bool refuseCall(const std::string& callId);
  bool holdCall(const std::string& callId);
  bool unholdCall(const std::string& callId);
  bool hangupCall(const std::string& callId);
  bool transferCall(const std::string& callId, const std::string& to);
  bool dtmfCall(const std::string& callId, const std::string& dtmfKey);
  bool hangupAll();
  bool getCurrentCallId(std::string& callId);

  std::string version();
  void quit() { _getEventsSequenceId="seq0"; _requestManager.quit(); }
  void stop() { _requestManager.stop(); }

  // observer methods
  void update();

  void setSessionPort(int port) {
    if(port>0 && port<65536) {_sessionPort=port;}
  };

private:
  void insertSubCall(const CallID& id, const std::string& seq);
  void removeSubCall(const CallID& id);
  std::string getSequenceIdFromId(const CallID& id);

  /**
   * This callMap is necessary because
   * because we want to retreive the seq associate to a call id
   * and also a sequence number
   */
  CallMap _callMap;

  // RequestManager execute received request 
  // and send response
  RequestManager _requestManager;

  std::string _getEventsSequenceId; // default is seq0
  int _sessionPort;
};

#endif // __GUI_SERVER_H__
