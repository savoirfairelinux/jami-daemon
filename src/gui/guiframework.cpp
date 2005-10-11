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

#include <string>
using namespace std;

#include "guiframework.h"
#include "../manager.h"

GuiFramework::GuiFramework ()
{}

GuiFramework::~GuiFramework (void) {}

int 
GuiFramework::outgoingCall (const string& to)
{
  return Manager::instance().outgoingCall(to);
}

int 
GuiFramework::hangupCall (short id)
{
	if (Manager::instance().hangupCall(id) == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::cancelCall (short id)
{
	if (Manager::instance().cancelCall(id) == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::answerCall (short id)
{
	if (Manager::instance().answerCall(id) == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::onHoldCall (short id)
{
	if (Manager::instance().onHoldCall(id) == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::offHoldCall (short id)
{
	if (Manager::instance().offHoldCall(id) == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::transferCall (short id, const string& to)
{
	if (Manager::instance().transferCall(id, to) == 0) {
		return 1;
	} else {
		return 0;
	}
}

void
GuiFramework::mute() 
{
  Manager::instance().mute();
}
void
GuiFramework::unmute() 
{
  Manager::instance().unmute();
}

void
GuiFramework::muteOn (short id)
{
	Manager::instance().muteOn(id);
}

void
GuiFramework::muteOff (short id)
{
	Manager::instance().muteOff(id);
}

int 
GuiFramework::refuseCall (short id)
{
	if (Manager::instance().refuseCall(id) == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::saveConfig (void)
{
	if (Manager::instance().saveConfig()) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::registerVoIPLink (void)
{
	if (Manager::instance().registerVoIPLink()) {
		return 1;
	} else {
		return 0;
	}
}

int 
GuiFramework::unregisterVoIPLink (void)
{
	if (Manager::instance().unregisterVoIPLink()) {
		return 1;
	} else {
		return 0;
	}
}

bool 
GuiFramework::sendDtmf (short id, char code)
{
	return Manager::instance().sendDtmf(id, code);
}

bool 
GuiFramework::playDtmf (char code)
{
	return Manager::instance().playDtmf(code);
}

bool 
GuiFramework::playTone ()
{
	return Manager::instance().playTone();
}

int 
GuiFramework::quitApplication (void)
{
	return (Manager::instance().quitApplication() ? 1 : 0);
}

int 
GuiFramework::sendTextMessage (short id, const string& message)
{
	Manager::instance().sendTextMessage(id, message);
	return 1;
}

int 
GuiFramework::accessToDirectory (void)
{
	Manager::instance().accessToDirectory();
	return 1;
}

/**
 * Configuration section / redirection
 */
bool 
GuiFramework::getZeroconf(const std::string& sequenceId) 
{
  return Manager::instance().getZeroconf(sequenceId);
}

bool 
GuiFramework::attachZeroconfEvents(const std::string& sequenceId, Pattern::Observer& observer)
{
  return Manager::instance().attachZeroconfEvents(sequenceId, observer);
}

bool 
GuiFramework::detachZeroconfEvents(Pattern::Observer& observer)
{
  return Manager::instance().detachZeroconfEvents(observer);
}

bool 
GuiFramework::getCallStatus(const std::string& sequenceId)
{
  return Manager::instance().getCallStatus(sequenceId);
}
short
GuiFramework::getCurrentId() 
{
  return Manager::instance().getCurrentCallId();
}

bool 
GuiFramework::getConfigAll(const std::string& sequenceId)
{
  return Manager::instance().getConfigAll(sequenceId);
}

bool 
GuiFramework::getConfig(const std::string& section, const std::string& name, TokenList& arg)
{
  return Manager::instance().getConfig(section, name, arg);
}

bool 
GuiFramework::setConfig(const std::string& section, const std::string& name, const std::string& value)
{
  return Manager::instance().setConfig(section, name, value);
}

bool 
GuiFramework::getConfigList(const std::string& sequenceId, const std::string& name)
{
  return Manager::instance().getConfigList(sequenceId, name);
}

