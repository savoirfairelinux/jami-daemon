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

#include "requestconfig.h"
#include "guiserver.h"
#include "subcall.h"

ResponseMessage
RequestGetEvents::execute()
{
  GUIServer::instance().getEvents(_sequenceId);
  return message("000", "OK");
}

ResponseMessage
RequestZeroconf::execute()
{
  if (GUIServer::instance().getZeroconf(_sequenceId)) {
    return message("200", "OK");
  } else {
    return message("501","Zeroconf not enabled or activated");
  }
}

ResponseMessage
RequestZeroconfEvent::execute()
{
  if (GUIServer::instance().attachZeroconfEvents(_sequenceId)) {
    return message("200", "OK");
  } else {
    return message("501","Zeroconf not enabled or activated");
  }
}

ResponseMessage
RequestCallStatus::execute()
{
  GUIServer::instance().sendGetEventsEnd(_sequenceId);
  TokenList tk;
  tk.push_back("OK");
  std::string code = "205";
  GUIServer::instance().getCallStatus(_sequenceId);

  std::string callid;
  if (GUIServer::instance().getCurrentCallId(callid)) {
    tk.push_front(callid);
  } else {
    code = "206";
  }
  return message(code, tk);
}

ResponseMessage
RequestConfigGetAll::execute()
{
  if (GUIServer::instance().getConfigAll(_sequenceId)) {
    return message("200", "OK");
  } else {
    return message("500","Server Error");
  }
}

RequestConfigGet::RequestConfigGet(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList)
{
  TokenList::iterator iter = _argList.begin();
  bool argsAreValid = false;
  if (iter != _argList.end()) {
    _section = *iter;
    _argList.pop_front();
    iter++;
    if (iter != _argList.end()) {
      _name = *iter;
      _argList.pop_front();
      argsAreValid = true;
    }
  }
  if (!argsAreValid) {
    throw RequestConstructorException();
  }
}

ResponseMessage
RequestConfigGet::execute()
{
  TokenList arg;
  if (GUIServer::instance().getConfig(_section, _name, arg)) {
    return message("200", arg);
  } else {
    //  402 seq10001 Variable unknown
    return message("402","Variable unknown");
  }
}

RequestConfigSet::RequestConfigSet(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList)
{
  TokenList::iterator iter = _argList.begin();

  // get three strings arguments
  bool argsAreValid = false;
  if (iter != _argList.end()) {
    _section = *iter;
    _argList.pop_front();
    iter++;
    if (iter != _argList.end()) {
      _name = *iter;
      _argList.pop_front();
      iter++;
      if (iter != _argList.end()) {
        _value = *iter;
        _argList.pop_front();
        argsAreValid = true;
      }
    }
  }
  if (!argsAreValid) {
    throw RequestConstructorException();
  }
}

ResponseMessage
RequestConfigSet::execute()
{
  if (GUIServer::instance().setConfig(_section, _name, _value)) {
    return message("200", "OK");
  } else {
    return message("500","Server Error");
  }
}

ResponseMessage
RequestConfigSave::execute()
{
  if (GUIServer::instance().saveConfig()) {
    return message("200", "Config saved");
  } else {
    return message("400","Error Unable to save the configuration");
  }
}

RequestList::RequestList(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList)
{
  TokenList::iterator iter = _argList.begin();
  if (iter != _argList.end()) {
    _name = *iter;
    _argList.pop_front();
  } else {
    throw RequestConstructorException();
  }
}

ResponseMessage
RequestList::execute()
{
  if (GUIServer::instance().getConfigList(_sequenceId, _name)) {
    return message("200", "OK");
  } else {
    return message("500","Server Error");
  }
}
