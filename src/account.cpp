/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#include "account.h"
#include "voiplink.h"
#include "manager.h"

#include <string>

Account::Account(const AccountID& accountID) : _accountID(accountID)
{
	_link = NULL;
	_enabled = false;
}

Account::~Account()
{
}

void
Account::loadConfig()
{
	_enabled = Manager::instance().getConfigInt(_accountID, CONFIG_ACCOUNT_ENABLE) ? true : false;
}

// NOW
void
Account::loadContacts()
{
	// TMP
	Contact* contact1 = new Contact("1223345", "Guillaume140", "<sip:140@asterix.inside.savoirfairelinux.net>");
	_contacts.push_back(contact1);
	Contact* contact2 = new Contact("9876543", "SFLphone131", "<sip:131@asterix.inside.savoirfairelinux.net>");
	_contacts.push_back(contact2);
	Contact* contact3 = new Contact("6867823", "Guillaume201", "<sip:201@192.168.1.202:5066>");
	_contacts.push_back(contact3);
	Contact* contact4 = new Contact("3417928", "SFLphone203", "<sip:203@192.168.1.202:5066>");
	_contacts.push_back(contact4);
	
	// TODO Load contact file containing list of contacts
	// or a configuration for LDAP contacts
}

void
Account::subscribeContactsPresence()
{
	if(_link->isContactPresenceSupported())
	{
		// Subscribe to presence for each contact that presence is enabled
		std::vector<Contact*>::iterator iter;
		
		for(iter = _contacts.begin(); iter != _contacts.end(); iter++)
		{
			_link->subscribePresenceForContact(*iter);
		}
	}
}

void
Account::publishPresence(std::string presenceStatus)
{
	if(_link->isContactPresenceSupported())
		_link->publishPresenceStatus(presenceStatus);
}
