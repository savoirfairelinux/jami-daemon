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

  void insertSubCall(short id, SubCall& subCall);
  void removeSubCall(short id);
  std::string getSequenceIdFromId(short id);
  short getIdFromCallId(const std::string& callId);

  // Reimplementation of virtual functions
	virtual int incomingCall (short id);
	virtual void peerAnsweredCall (short id);
	virtual int peerRingingCall (short id);
	virtual int peerHungupCall (short id);
	virtual void displayTextMessage (short id, const std::string& message);
	virtual void displayErrorText (short id, const std::string& message);
	virtual void displayError (const std::string& error);
	virtual void displayStatus (const std::string& status);
	virtual void displayContext (short id);
	virtual std::string getRingtoneFile (void);
	virtual void setup (void);
	virtual void startVoiceMessageNotification (void);
	virtual void stopVoiceMessageNotification (void);  

  bool outgoingCall (const std::string& seq, 
    const std::string& callid, 
    const std::string& to);

  void hangup(const std::string& callId);
  void quit() {_shouldQuit=true;}

private:

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
  bool _shouldQuit;
};

#endif // __GUI_SERVER_H__
