/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
 
#ifndef CALLMANAGER_H
#define CALLMANAGER_H

#include "callmanager-glue.h"
#include <dbus-c++/dbus.h>

    
class CallManager
: public org::sflphone::SFLphone::CallManager_adaptor,
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
    void placeCall( const std::string& accountID, const std::string& callID, const std::string& to );
    void refuse( const std::string& callID );
    void accept( const std::string& callID );
    void hangUp( const std::string& callID );
    void hold( const std::string& callID );
    void unhold( const std::string& callID );
    void transfert( const std::string& callID, const std::string& to );
    void setVolume( const std::string& device, const double& value );
    double getVolume( const std::string& device );
    void joinParticipant( const std::string& sel_callID, const std::string& drag_callID );
    void detachParticipant( const std::string& callID );
    void setRecording( const std::string& callID );
    bool getIsRecording(const std::string& callID);
    std::string getCurrentCodecName(const std::string& callID);
    
    std::map< std::string, std::string > getCallDetails( const std::string& callID );
    std::vector< std::string > getCallList (void);

    std::string getCurrentCallID(  );
    void playDTMF( const std::string& key );
    void startTone( const int32_t& start, const int32_t& type );
    
};


#endif//CALLMANAGER_H
