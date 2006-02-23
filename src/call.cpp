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

#include <iostream>

#include "call.h"
#include "voIPLink.h"

Call::Call (CALLID id, Call::CallType type, VoIPLink* voiplink)
{
	_state = NotExist;
	_type = Null;
	_id = id; 
	_type = type;
	_voIPLink = voiplink;
  _flagNotAnswered = true;
	
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

CALLID
Call::getId (void)
{
	return _id;
}

void 
Call::setId (CALLID id)
{
	_id = id;
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

std::string 
Call::getCallerIdName (void)
{
	return _callerIdName;
}

void 
Call::setCallerIdName (const std::string& callerId_name)
{
	_callerIdName = callerId_name;
}

std::string 
Call::getCallerIdNumber (void)
{
	return _callerIdNumber;
}

void 
Call::setCallerIdNumber (const std::string& callerId_number)
{
	_callerIdNumber = callerId_number;
}

Call::CallState
Call::getState (void)
{
	return _state;  
}

void 
Call::setState (Call::CallState state) 
{
	_state = state;
}

Call::CallType 
Call::getType (void)
{
	return _type;
}

void 
Call::setType (Call::CallType type)
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
Call::isAnswered (void)
{
	return (_state == Answered) ? true : false;
}

bool 
Call::isNotAnswered (void)
{
	return (_state == Error || _state == NotExist || _state == Busy) ? true : false;
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
Call::outgoingCall(const std::string& to)
{
	return _voIPLink->outgoingInvite(_id, to);
}

int 
Call::hangup  (void)
{
	int i = _voIPLink->hangup(_id);
  setState(Hungup);
	return i;
}

int 
Call::cancel  (void)
{
	int i = _voIPLink->cancel(_id);
  setState(Hungup);
	return i;
}

int 
Call::answer  (void)
{
  _flagNotAnswered = false;
	int i = _voIPLink->answer(_id);
  setState(Answered);
	return i;
}

int 
Call::onHold  (void)
{
	int i = _voIPLink->onhold(_id);
  setState(OnHold);
	return i;
}

int 
Call::offHold  (void)
{
	int i = _voIPLink->offhold(_id);
  setState(OffHold);
	return i;
}

int 
Call::transfer  (const std::string& to)
{
	int i = _voIPLink->transfer(_id, to);
  setState(Transfered);
	return i;
}

int 
Call::refuse  (void)
{
	int i = _voIPLink->refuse(_id);
	return i;
}
