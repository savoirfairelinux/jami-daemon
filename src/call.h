/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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

#ifndef __CALL_H__
#define __CALL_H__

#include <string>

typedef unsigned int CALLID;

enum CallType {
	Null = 0,
	Incoming,
	Outgoing
};


class VoIPLink;

class Call {
public:
  enum CallState {
    NotExist = 0,
    Busy,
    OnHold,
    OffHold,
    MuteOn,
    MuteOff,
    Transfered,
    Hungup,
    Answered,
    Ringing,
    Progressing,
    Refused,	// for refuse incoming ringing call	
    Error     // when a error occur
  };

	// Constructor
	Call(CALLID id, CallType type, VoIPLink* voiplink);
	// Destructor
	~Call(void);
	

	// Handle call-id
	CALLID getId (void);
	void setId (CALLID id);
	
	// Accessor and modifior of VoIPLink
	VoIPLink* getVoIPLink(void);
	void setVoIPLink (VoIPLink* voIPLink);
		
	// Handle id name and id number
	std::string getCallerIdName (void);
	void setCallerIdName (const std::string& callerId_name);
	std::string getCallerIdNumber (void);
	void setCallerIdNumber (const std::string& callerId_number);
 	
	// Handle state
	CallState getState (void);
	void setState (CallState state);
	
	// Handle type of call (incoming or outoing)
	enum CallType getType (void);
	void setType (enum CallType type);

  bool isNotAnswered(void);
	bool isBusy			  (void);
	bool isOnHold 		(void);
	bool isOffHold 		(void);
	bool isOnMute 		(void);
	bool isOffMute 		(void);
	bool isTransfered 	(void);
	bool isHungup 		(void);
	bool isRinging 		(void);
	bool isRefused 		(void);
	bool isAnswered 	(void);
	bool isProgressing 	(void);
	bool isOutgoingType (void);
	bool isIncomingType (void);
	
	int outgoingCall  	(const std::string& to);
	int hangup  		(void);
	int cancel  		(void);
	int answer  		(void);
	int onHold  		(void);
	int offHold  		(void);
	int transfer  		(const std::string& to);
	int refuse  		(void);
  void setFlagNotAnswered(bool value) { _flagNotAnswered = value; }
  bool getFlagNotAnswered() const { return _flagNotAnswered; }

private:
	VoIPLink		*_voIPLink;	
	CALLID 		  	 _id;
	enum CallState 	 _state;
	enum CallType 	 _type;
	std::string 			 _callerIdName;
	std::string 			 _callerIdNumber;

  bool _flagNotAnswered;
};

#endif // __CALL_H__
