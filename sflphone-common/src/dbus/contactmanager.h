/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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
 
#ifndef CONTACTMANAGER_H
#define CONTACTMANAGER_H

#include "contactmanager-glue.h"
#include <dbus-c++/dbus.h>

    
class ContactManager
: public org::sflphone::SFLphone::ContactManager_adaptor,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

    ContactManager(DBus::Connection& connection);
    static const char* SERVER_PATH;

public:
    std::map< std::string, std::string > getContacts( const std::string& accountID );
    void setContacts( const std::string& accountID, const std::map< std::string, std::string >& details );
    void setPresence( const std::string& accountID, const std::string& presence, const std::string& additionalInfo );
    void setContactPresence( const std::string& accountID, const std::string& presence, const std::string& additionalInfo );

};


#endif//CONTACTMANAGER_H
