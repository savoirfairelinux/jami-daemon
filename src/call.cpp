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

#include <iostream>
#include "call.h"
#include "manager.h"
#include "sipvoiplink.h"
#include "voIPLink.h"


Call::Call (Manager* manager, short id, CallType type, VoIPLink* voiplink)
{
 	initConstructor();
	_id = id; 
	_type = type;
	_manager = manager;
	_voIPLink = voiplink;
	
	switch (_type) {
	case Outgoing:
		_voIPLink->newOutgoingCall(_id);
		break;
	case Incoming:
		_voIPLink->newIncomingCall(_id);
		break;
	default:
		break;
	}
}

Call::~Call (void)
{
}

short
Call::getId (void)
{
	return _id;
}

void 
Call::setId (short id)
{
	_id = id;
}

unsigned int 
Call::getTimestamp(void)
{
	return _timestamp;
}

void 
Call::setTimestamp (unsigned int timestamp)
{
	_timestamp = timestamp;
}

short
Call::getVoIPLinkId (void)
{
	return _voIPLinkId;
}

void 
Call::setVoIPLinkId (short voIPLinkId)
{
	_voIPLinkId = voIPLinkId;
}

void 
Call::setVoIPLink (VoIPLink* voIPLink)
{
	_voIPLink = voIPLink;
}

VoIPLink*
Call::getVoIPLink (void)
{
	return _voIPLink;
}

string 
Call::getStatus (void)
{
	return _status;
}

void 
Call::setStatus (const string& status)
{
	_status = status;
}

string 
Call::getTo (void)
{
	return _to;
}

void 
Call::setTo (const string& to)
{
	_to = to;
}

string 
Call::getCallerIdName (void)
{
	return _callerIdName;
}

void 
Call::setCallerIdName (const string& callerId_name)
{
	_callerIdName = callerId_name;
}

string 
Call::getCallerIdNumber (void)
{
	return _callerIdNumber;
}

void 
Call::setCallerIdNumber (const string& callerId_number)
{
	_callerIdNumber = callerId_number;
}

enum CallState 
Call::getState (void)
{
	return _state;  
}

void 
Call::setState (enum CallState state) 
{
	_state = state;
}

enum CallType 
Call::getType (void)
{
	return _type;
}

void 
Call::setType (enum CallType type)
{
	_type = type;
}

bool
Call::isBusy (void)
{
	if (isAnswered() or isOffHold() or isOnMute() or isOffMute()) {
		return true;
	} else {
		return false;
	}
}
bool 
Call::isOnHold (void)
{
	return (_state == OnHold) ? true : false;
}

bool 
Call::isOffHold (void)
{
	return (_state == OffHold) ? true : false;
}

bool 
Call::isOnMute (void)
{
	return (_state == MuteOn) ? true : false;
}

bool 
Call::isOffMute (void)
{
	return (_state == MuteOff) ? true : false;
}

bool 
Call::isTransfered (void)
{
	return (_state == Transfered) ? true : false;
}

bool 
Call::isHungup (void)
{
	return (_state == Hungup) ? true : false;
}

bool 
Call::isRinging (void)
{
	return (_state == Ringing) ? true : false;
}

bool 
Call::isRefused (void)
{
	return (_state == Refused) ? true : false;
}

bool 
Call::isCancelled (void)
{
	return (_state == Cancelled) ? true : false;
}

bool 
Call::isAnswered (void)
{
	return (_state == Answered) ? true : false;
}

bool 
Call::isProgressing (void)
{
	return (_state == Progressing) ? true : false;
}

bool
Call::isOutgoingType (void)
{
	return (_type == Outgoing) ? true : false;
}

bool
Call::isIncomingType (void)
{
	return (_type == Incoming) ? true : false;
}

int 
Call::outgoingCall  (const string& to)
{
	return _voIPLink->outgoingInvite(to);
}

int 
Call::hangup  (void)
{
	int i = _voIPLink->hangup(_id);
	_voIPLink->deleteSipCall(_id);
	return i;
}

int 
Call::answer  (void)
{
	int i = _voIPLink->answer(_id);
	return i;
}

int 
Call::onHold  (void)
{
	int i = _voIPLink->onhold(_id);
	return i;
}

int 
Call::offHold  (void)
{
	int i = _voIPLink->offhold(_id);
	return i;
}

int 
Call::transfer  (const string& to)
{
	int i = _voIPLink->transfer(_id, to);
	return i;
}

int 
Call::muteOn (void)
{
	return 1;
}

int 
Call::muteOff (void)
{
	return 1;
}

int 
Call::refuse  (void)
{
	int i = _voIPLink->refuse(_id);
	_voIPLink->deleteSipCall(_id);
	return i;
}


///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
void
Call::initConstructor(void)
{
	_timestamp = 0;
	_state = NotExist;
	_type = Null;
	_voIPLinkId = 1;
}
