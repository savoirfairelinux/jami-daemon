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

#include <contactmanager.h>
#include "../manager.h"

const char* ContactManager::SERVER_PATH = "/org/sflphone/SFLphone/ContactManager";

std::map< ::DBus::String, ::DBus::String >
ContactManager::getContacts( const ::DBus::String& accountID UNUSED)
{
        // TODO
    std::map< ::DBus::String, ::DBus::String > ret;
    return ret;
}

void
ContactManager::setContacts( const ::DBus::String& accountID UNUSED, const std::map< ::DBus::String UNUSED, ::DBus::String >& details UNUSED)
{
	// TODO
}

void
ContactManager::setPresence( const ::DBus::String& accountID UNUSED, const ::DBus::String& presence UNUSED, const ::DBus::String& additionalInfo UNUSED)
{
	// TODO
}

void
ContactManager::setContactPresence( const ::DBus::String& accountID UNUSED, const ::DBus::String& presence UNUSED, const ::DBus::String& additionalInfo UNUSED)
{
	// TODO
}
