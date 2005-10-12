/**
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

#include "subcall.h"
#include "requestmanager.h"

typedef std::map<short, SubCall> CallMap;

class GUIServerImpl : public GuiFramework {
public:
  // GUIServerImpl constructor
  GUIServerImpl();
  // GUIServerImpl destructor
  ~GUIServerImpl();
  
  // exec loop
  int exec(void);

  // Reimplementation of virtual functions
  // TODO: remove incomingCall with one parameter
	int incomingCall (short id);
  int incomingCall(short id, const std::string& accountId, const std::string& from);

	void peerAnsweredCall (short id);
	int peerRingingCall (short id);
	int peerHungupCall (short id);
	void displayTextMessage (short id, const std::string& message);
	void displayErrorText (short id, const std::string& message);
	void displayError (const std::string& error);
	void displayStatus (const std::string& status);
	void displayContext (short id);
	void setup (void);
	//void startVoiceMessageNotification (void);
	//void stopVoiceMessageNotification (void);
  void sendVoiceNbMessage(const std::string& nb_msg);

  void sendMessage(const std::string& code, const std::string& seqId, TokenList&
arg);
  void sendCallMessage(const std::string& code, 
  const std::string& sequenceId, 
  short id, 
  TokenList arg);
  void callFailure(short id);

  bool getEvents(const std::string& sequenceId);
  bool sendGetEventsEnd();

  bool outgoingCall (const std::string& seq, 
    const std::string& callid, 
    const std::string& to);
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
  void quit() { _requestManager.quit(); }
  void stop() { _requestManager.stop(); }

  // observer methods
  void update();

private:
  void insertSubCall(short id, SubCall& subCall);
  void removeSubCall(short id);
  std::string getSequenceIdFromId(short id);
  std::string getCallIdFromId(short id);
  short getIdFromCallId(const std::string& callId);

  /**
   * This callMap is necessary because
   * ManagerImpl use callid-int
   * and the client use a  callid-string
   * and also a sequence number
   */
  CallMap _callMap;

  // RequestManager execute received request 
  // and send response
  RequestManager _requestManager;

  std::string _getEventsSequenceId; // default is seq0
};

#endif // __GUI_SERVER_H__
