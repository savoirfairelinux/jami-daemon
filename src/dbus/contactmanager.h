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
: public org::sflphone::SFLphone::ContactManager,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

    ContactManager(DBus::Connection& connection);
    static const char* SERVER_PATH;

public:
    std::map< ::DBus::String, ::DBus::String > getContacts( const ::DBus::String& accountID );
    void setContacts( const ::DBus::String& accountID, const std::map< ::DBus::String, ::DBus::String >& details );
    void setPresence( const ::DBus::String& accountID, const ::DBus::String& presence, const ::DBus::String& additionalInfo );
    void setContactPresence( const ::DBus::String& accountID, const ::DBus::String& presence, const ::DBus::String& additionalInfo );

};


#endif//CONTACTMANAGER_H
