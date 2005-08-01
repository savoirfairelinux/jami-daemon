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

#include <qpushbutton.h>

#include "jpushbutton.h"
#include "phoneline.h"

PhoneLine::PhoneLine (void) {
	jpb = NULL;
	setState (FREE);
	first = true;
	timer = new QTime();	
	b_ringing = false;
	_callid = 0;
	_status = "";
	_scrolling = false;
	_stopScrolling = false;
}

PhoneLine::~PhoneLine (void) {
	if (timer != NULL) {
		delete timer;
	}
}

enum line_state
PhoneLine::getState (void) {
	return state;
}

void
PhoneLine::setState (enum line_state state) {
	this->state = state;
}

bool
PhoneLine::isFree (void) {
	if (getState() == FREE) {
		return true;
	} else {
		return false;
	}
}

bool
PhoneLine::isBusy (void) {
	if (getState() == BUSY) {
		return true;
	} else {
		return false;
	}
}

bool
PhoneLine::isOnHold (void) {
	if (getState() == ONHOLD) {
		return true;
	} else {
		return false;
	}
}

void
PhoneLine::toggleState (void) {
	if (isBusy()){
		setState(ONHOLD);
	} else if (isFree() || isOnHold()) {
		setState (BUSY);
	} 
}

void
PhoneLine::setButton (JPushButton *jpb) {
	this->jpb = jpb;
}

JPushButton *
PhoneLine::button (void) {
	return this->jpb;
}

void
PhoneLine::startTimer(void) {
	if (timer == NULL) 
		timer = new QTime();
	timer->start();
}

void
PhoneLine::stopTimer(void) {
	delete timer;
	timer = NULL;
}


// EOF
