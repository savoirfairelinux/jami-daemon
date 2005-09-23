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

#include "request.h"
#include "guiserver.h"
#include "subcall.h"

ResponseMessage
RequestCall::execute()
{
  if ( GUIServer::instance().outgoingCall(_sequenceId, _callId, _destination) ) {
    return message("150", "Trying");
  } else {
    return message("500","Server Error");
  }
}

ResponseMessage
RequestAnswer::execute()
{
  return message("200","TODO");
}

ResponseMessage
RequestRefuse::execute()
{
  return message("200","TODO");
}

ResponseMessage
RequestHold::execute()
{
  return message("200","TODO");
}

ResponseMessage
RequestUnhold::execute()
{
  return message("200","TODO");
}

ResponseMessage
RequestTransfer::execute()
{
  return message("200","TODO");
}

ResponseMessage
RequestHangup::execute()
{
  try {
    GUIServer::instance().hangup(_callId);
    return message("200", "OK");
  } catch (...) {
    return message("500", "Hangup Error");
  }
}


ResponseMessage
RequestMute::execute()
{
  return message("200","TODO");
}

ResponseMessage
RequestUnmute::execute()
{
  return message("200","TODO");
}

ResponseMessage
RequestQuit::execute()
{
  GUIServer::instance().quit();
  return message("200", "Quitting");
}

