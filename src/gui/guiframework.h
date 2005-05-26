/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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
using namespace std;

#include "../manager.h"

class GuiFramework {
public:
	GuiFramework (Manager* manager);
	virtual ~GuiFramework (void);

	/* Parent class to child class */
	virtual int incomingCall (short id) = 0;
	virtual int peerAnsweredCall (short id) = 0;
	virtual int peerRingingCall (short id) = 0;
	virtual int peerHungupCall (short id) = 0;
	virtual void displayTextMessage (short id, const string& message) = 0;
	virtual void displayError (const string& error) = 0;
	virtual void displayStatus (const string& status) = 0;
	virtual void displayContext (short id) = 0;
	virtual string getRingtoneFile (void) = 0;
	virtual void setup (void) = 0;
	virtual int selectedCall (void) = 0;
	
	/* Child class to parent class */
	int outgoingCall (const string& to); 	
	int hangupCall (short id);
	int answerCall (short id);
	int onHoldCall (short id);
	int offHoldCall (short id);
	int transferCall (short id, const string& to);
	int muteOn (short id);
	int muteOff (short id);
	int refuseCall (short id);
	int cancelCall (short id);

	int saveConfig (void);
	int registerVoIPLink (void);
	int quitApplication (void);
	int sendTextMessage (short id, const string& message);
	int accessToDirectory (void);
	void sendDtmf (short id, char code);
	
protected:
	string _message;

private:	
	Manager* _manager;
};

#endif // __GUI_FRAMEWORK_H__
