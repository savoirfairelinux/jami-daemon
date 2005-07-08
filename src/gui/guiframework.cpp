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

#include <string>
using namespace std;

#include "guiframework.h"
#include "../manager.h"

GuiFramework::GuiFramework (Manager* manager)
{
	_manager = manager;
}

GuiFramework::~GuiFramework (void) {}

int 
GuiFramework::outgoingCall (const string& to)
{
	return _manager->outgoingCall(to);
}

int 
GuiFramework::hangupCall (short id)
{
	if (_manager->hangupCall(id)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::cancelCall (short id)
{
	if (_manager->cancelCall(id)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::answerCall (short id)
{
	if (_manager->answerCall(id)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::onHoldCall (short id)
{
	if (_manager->onHoldCall(id)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::offHoldCall (short id)
{
	if (_manager->offHoldCall(id)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::transferCall (short id, const string& to)
{
	if (_manager->transferCall(id, to)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::muteOn (short id)
{
	if (_manager->muteOn(id)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::muteOff (short id)
{
	if (_manager->muteOff(id)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::refuseCall (short id)
{
	if (_manager->refuseCall(id)) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::saveConfig (void)
{
	if (_manager->saveConfig()) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::registerVoIPLink (void)
{
	if (_manager->registerVoIPLink()) {
		return 1;
	} else {
		return 0;
	}
}

void 
GuiFramework::sendDtmf (short id, char code)
{
	_manager->sendDtmf(id, code);
}

int 
GuiFramework::quitApplication (void)
{
	return (_manager->quitApplication() ? 1 : 0);
}

int 
GuiFramework::sendTextMessage (short id, const string& message)
{
	_manager->sendTextMessage(id, message);
	return 1;
}

int 
GuiFramework::accessToDirectory (void)
{
	_manager->accessToDirectory();
	return 1;
}

