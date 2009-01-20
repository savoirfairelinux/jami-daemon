/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
#include <global.h>
#include <callmanager.h>
#include "../manager.h"

const char* CallManager::SERVER_PATH = "/org/sflphone/SFLphone/CallManager";

CallManager::CallManager( DBus::Connection& connection )
: DBus::ObjectAdaptor(connection, SERVER_PATH)
{
}

void
CallManager::placeCall( const std::string& accountID, 
                        const std::string& callID,          
                        const std::string& to )
{
    _debug("CallManager::placeCall received\n");
    // Check if a destination number is available
    if( to == "")   _debug("No number entered - Call stopped\n"); 
    else            Manager::instance().outgoingCall(accountID, callID, to);
}

void
CallManager::refuse( const std::string& callID )
{
    _debug("CallManager::refuse received\n");
    Manager::instance().refuseCall(callID);
}

void
CallManager::accept( const std::string& callID )
{
    _debug("CallManager::accept received\n");
    Manager::instance().answerCall(callID);
}

void
CallManager::hangUp( const std::string& callID )
{
    _debug("CallManager::hangUp received\n");
    Manager::instance().hangupCall(callID);

}

void
CallManager::hold( const std::string& callID )
{
    _debug("CallManager::hold received\n");
    Manager::instance().onHoldCall(callID);
    
}

void
CallManager::unhold( const std::string& callID )
{
    _debug("CallManager::unhold received\n");
    Manager::instance().offHoldCall(callID);
}

void
CallManager::transfert( const std::string& callID, const std::string& to )
{
    _debug("CallManager::transfert received\n");
    Manager::instance().transferCall(callID, to);
}

void
CallManager::setVolume( const std::string& device, const double& value )
{
    _debug("CallManager::setVolume received\n");
    if(device == "speaker")
    {
      Manager::instance().setSpkrVolume((int)(value*100.0));
    }
    else if (device == "mic")
    {
      Manager::instance().setMicVolume((int)(value*100.0));
    }
    volumeChanged(device, value);
}

double 
CallManager::getVolume( const std::string& device )
{
    _debug("CallManager::getVolume received\n");
    if(device == "speaker")
    {
      _debug("Current speaker = %d\n", Manager::instance().getSpkrVolume());
      return Manager::instance().getSpkrVolume()/100.0;
    }
    else if (device == "mic")
    {
      _debug("Current mic = %d\n", Manager::instance().getMicVolume());
      return Manager::instance().getMicVolume()/100.0;
    }
    return 0;
}

std::map< std::string, std::string > 
CallManager::getCallDetails( const std::string& callID UNUSED )
{
    _debug("CallManager::getCallDetails received\n");
    std::map<std::string, std::string> a;
    return a;
}

std::string 
CallManager::getCurrentCallID(  )
{
    _debug("CallManager::getCurrentCallID received\n");
    return Manager::instance().getCurrentCallId();
}

void 
CallManager::playDTMF( const std::string& key )
{
  Manager::instance().sendDtmf(Manager::instance().getCurrentCallId(), key.c_str()[0]);
}

void 
CallManager::startTone( const int32_t& start , const int32_t& type )
{
  if( start == true )
  {
    if( type == 0 )
      Manager::instance().playTone();
    else
      Manager::instance().playToneWithMessage();
  }
  else
    Manager::instance().stopTone(true);
}

