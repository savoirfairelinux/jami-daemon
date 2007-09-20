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
 
#ifndef CALLMANAGER_H
#define CALLMANAGER_H

#include "callmanager-glue.h"
#include <dbus-c++/dbus.h>

    
class CallManager
: public org::sflphone::SFLphone::CallManager,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

    CallManager(DBus::Connection& connection);
    static const char* SERVER_PATH;

public:

    /* methods exported by this interface,
     * you will have to implement them in your ObjectAdaptor
     */
    void placeCall( const ::DBus::String& accountID, const ::DBus::String& callID, const ::DBus::String& to );
    void refuse( const ::DBus::String& callID );
    void accept( const ::DBus::String& callID );
    void hangUp( const ::DBus::String& callID );
    void hold( const ::DBus::String& callID );
    void unhold( const ::DBus::String& callID );
    void transfert( const ::DBus::String& callID, const ::DBus::String& to );
    void setVolume( const ::DBus::String& device, const ::DBus::Double& value );
    ::DBus::Double getVolume( const ::DBus::String& device );
    std::map< ::DBus::String, ::DBus::String > getCallDetails( const ::DBus::String& callID );
    ::DBus::String getCurrentCallID(  );
    void playDTMF( const ::DBus::String& key );
    
};


#endif//CALLMANAGER_H
