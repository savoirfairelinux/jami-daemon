/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
 
#include <callmanager.h>

const char* CallManager::SERVER_PATH = "/org/sflphone/SFLPhone/CallManager";

CallManager::CallManager( DBus::Connection& connection )
: DBus::ObjectAdaptor(connection, SERVER_PATH)
{
}

void
CallManager::placeCall( const ::DBus::String& accountID, 
                        const ::DBus::String& callID,          
                        const ::DBus::String& to )
{
    _debug("CallManager::placeCall received\n");

}

void
CallManager::refuse( const ::DBus::String& callID )
{
    _debug("CallManager::refuse received\n");

}

void
CallManager::accept( const ::DBus::String& callID )
{
    _debug("CallManager::accept received\n");

}

void
CallManager::hangUp( const ::DBus::String& callID )
{
    _debug("CallManager::hangUp received\n");

}

void
CallManager::hold( const ::DBus::String& callID )
{
    _debug("CallManager::hold received\n");

}

void
CallManager::unhold( const ::DBus::String& callID )
{
    _debug("CallManager::unhold received\n");

}

void
CallManager::transfert( const ::DBus::String& callID, const ::DBus::String& to )
{
    _debug("CallManager::transfert received\n");

}

void
CallManager::setVolume( const ::DBus::String& device, const ::DBus::Double & value )
{
    _debug("CallManager::setVolume received\n");

}

::DBus::Double 
CallManager::getVolume( const ::DBus::String& device )
{
    _debug("CallManager::getVolume received\n");
    return 0;
}

::DBus::Int32 
CallManager::getVoiceMailCount(  )
{
    _debug("CallManager::getVoiceMailCount received\n");
    return 0;
}

std::map< ::DBus::String, ::DBus::String > 
CallManager::getCallDetails( const ::DBus::String& callID )
{
    _debug("CallManager::getCallDetails received\n");
    std::map<std::string, std::string> a;
    return a;
}

::DBus::String 
CallManager::getCurrentCallID(  )
{
    _debug("CallManager::getCurrentCallID received\n");
    return "test";
}


