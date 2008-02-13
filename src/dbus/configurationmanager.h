/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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
 
#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include "configurationmanager-glue.h"
#include <dbus-c++/dbus.h>

    
class ConfigurationManager
: public org::sflphone::SFLphone::ConfigurationManager,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

    ConfigurationManager(DBus::Connection& connection);
    static const char* SERVER_PATH;

public:

    std::map< ::DBus::String, ::DBus::String > getAccountDetails( const ::DBus::String& accountID );
    void setAccountDetails( const ::DBus::String& accountID, const std::map< ::DBus::String, ::DBus::String >& details );
    void addAccount( const std::map< ::DBus::String, ::DBus::String >& details );
    void removeAccount( const ::DBus::String& accoundID );
    std::vector< ::DBus::String > getAccountList(  );
    ::DBus::String getDefaultAccount(  );
    void setDefaultAccount( const ::DBus::String& accountID  );
    
    std::vector< ::DBus::String > getCodecList(  );
    std::vector< ::DBus::String > getCodecDetails( const ::DBus::Int32& payload );
    std::vector< ::DBus::String > getActiveCodecList(  );
    void setActiveCodecList( const std::vector< ::DBus::String >& list );
    
    std::vector< ::DBus::String > getAudioManagerList();
    void setAudioManager(const ::DBus::String& audioManager);
    std::vector< ::DBus::String > getAudioOutputDeviceList();
    void setAudioOutputDevice(const ::DBus::Int32& index);
    std::vector< ::DBus::String > getAudioInputDeviceList();
    void setAudioInputDevice(const ::DBus::Int32& index);
    std::vector< ::DBus::String > getCurrentAudioDevicesIndex();
    std::vector< ::DBus::String > getAudioDeviceDetails(const ::DBus::Int32& index);
   
    std::vector< ::DBus::String > getToneLocaleList(  );
    std::vector< ::DBus::String > getPlaybackDeviceList(  );
    std::vector< ::DBus::String > getRecordDeviceList(  );
    ::DBus::String getVersion(  );
    std::vector< ::DBus::String > getRingtoneList(  );

};


#endif//CONFIGURATIONMANAGER_H
