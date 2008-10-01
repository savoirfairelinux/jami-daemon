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

std::map< std::string, std::string >
ContactManager::getContacts( const std::string& accountID )
{
	// TODO
}

void
ContactManager::setContacts( const std::string& accountID, const std::map< std::string, std::string >& details )
{
	// TODO
}

void
ContactManager::setPresence( const std::string& accountID, const std::string& presence, const std::string& additionalInfo )
{
	// TODO
}

void
ContactManager::setContactPresence( const std::string& accountID, const std::string& presence, const std::string& additionalInfo )
{
	// TODO
}
