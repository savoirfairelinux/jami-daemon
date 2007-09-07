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
 
#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include "configurationmanager-glue.h"
#include <dbus-c++/dbus.h>

    
class ConfigurationManager
: public org::sflphone::SFLPhone::ConfigurationManager,
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
    ::DBus::String getVersion(  );
    std::vector< ::DBus::String > getRingtoneList(  );
    std::vector< ::DBus::String > getCodecList(  );
    std::vector< ::DBus::String > getToneLocaleList(  );
    void setCodecPreferedOrder( const std::vector< ::DBus::String >& ringtone );
    std::vector< ::DBus::String > getCodecPreferedOrder(  );
    std::vector< ::DBus::String > getPlaybackDeviceList(  );
    std::vector< ::DBus::String > getRecordDeviceList(  );
    std::vector< ::DBus::String > getSampleRateList(  );
    std::map< ::DBus::String, ::DBus::String > getParameters(  );
    void setParameters( const std::map< ::DBus::String, ::DBus::String >& parameters );

};


#endif//CONFIGURATIONMANAGER_H
