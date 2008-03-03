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

#ifndef CONTACT_H
#define CONTACT_H

#include "presence.h"

#include <string>

typedef std::string ContactID;

/**
 * TOCOMMENT
 * @author Guillaume Carmel-Archambault
 */
class Contact {
public:
	Contact();
	Contact(const std::string contactID, const std::string name, const std::string url);
	virtual ~Contact();
	
	std::string getUrl() { return _url; }
	
protected:
	
private:
	ContactID _contactID;
	std::string _name;
	std::string _url;
	bool _suscribeToPresence;
	
	// Presence information, can be null
	Presence* _presence;
};

#endif
