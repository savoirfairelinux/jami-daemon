/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
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
#include <libintl.h>

#include "global.h"
#include "guiframework.h"
#include "../manager.h"

GuiFramework::GuiFramework ()
{}

GuiFramework::~GuiFramework (void) {}

bool
GuiFramework::outgoingCall(const std::string& account, const CallID& id, const std::string& to)
{
  return Manager::instance().outgoingCall(account, id, to);
}

bool
GuiFramework::sendTextMessage(const std::string& account, const std::string& to, const std::string& message)
{
  return Manager::instance().sendTextMessage(account, to, message);
}

bool
GuiFramework::hangupCall (const CallID& id)
{
  return Manager::instance().hangupCall(id);
}

bool
GuiFramework::cancelCall (const CallID& id)
{
	return Manager::instance().cancelCall(id);
}

bool
GuiFramework::answerCall(const CallID& id)
{
  return Manager::instance().answerCall(id);
}

bool
GuiFramework::onHoldCall(const CallID& id)
{
  return Manager::instance().onHoldCall(id);
}

bool
GuiFramework::offHoldCall(const CallID& id)
{
  return Manager::instance().offHoldCall(id);
}

bool
GuiFramework::transferCall(const CallID& id, const std::string& to)
{
	return Manager::instance().transferCall(id, to);
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

bool
GuiFramework::refuseCall (const CallID& id)
{
	return Manager::instance().refuseCall(id);
}

bool
GuiFramework::saveConfig (void)
{
	return Manager::instance().saveConfig();
}

bool 
GuiFramework::registerVoIPLink(const AccountID& accountId)
{
  return Manager::instance().registerVoIPLink(accountId);
}

bool 
GuiFramework::unregisterVoIPLink (const AccountID& accountId)
{
  return Manager::instance().unregisterVoIPLink(accountId);
}

bool 
GuiFramework::sendDtmf (const CallID& id, char code)
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

bool 
GuiFramework::stopTone ()
{
  Manager::instance().stopTone();
  return true;
}

/**
 * Initialization section / redirection
 */
bool
GuiFramework::getEvents() 
{
  return Manager::instance().getEvents();
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

const CallID&
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

bool 
GuiFramework::setSpkrVolume(int volume)
{
  Manager::instance().setSpkrVolume(volume);
  return true;
}

bool 
GuiFramework::setMicVolume(int volume)
{
  Manager::instance().setMicVolume(volume);
  return true;
}

int
GuiFramework::getSpkrVolume()
{
  return Manager::instance().getSpkrVolume();
}

int
GuiFramework::getMicVolume()
{
  return Manager::instance().getMicVolume();
}

bool
GuiFramework::hasLoadedSetup()
{
  return Manager::instance().hasLoadedSetup();
}

bool 
GuiFramework::getRegistrationState(std::string& stateCode, std::string& stateMessage) {
  ManagerImpl::REGISTRATION_STATE state = Manager::instance().getRegistrationState();
  bool returnValue = false;
  switch( state ) {
    case ManagerImpl::REGISTERED:
      returnValue = true;
      stateCode    = "103";
      stateMessage = _("Registration succeed");
    break;

    case ManagerImpl::FAILED:
      returnValue = true;
      stateCode    = "104";
      stateMessage = _("Registration failed");
    break;

    case ManagerImpl::UNREGISTERED:
      returnValue = false;
      stateCode    = "";
      stateMessage = "";
    break;
  }
  return returnValue;
}

bool 
GuiFramework::setSwitch(const std::string& switchName, std::string& returnMessage)
{
  return Manager::instance().setSwitch(switchName, returnMessage);
}
